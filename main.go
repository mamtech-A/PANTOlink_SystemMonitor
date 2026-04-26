package main

import (
	"context"
	"encoding/json"
	"fmt"
	"log"
	"sync"
	"time"

	zmq "github.com/go-zeromq/zmq4"
	"github.com/shirou/gopsutil/cpu"
	"github.com/shirou/gopsutil/disk"
	"github.com/shirou/gopsutil/host"
	"github.com/shirou/gopsutil/mem"
	"github.com/shirou/gopsutil/net"
)

const (
	ClientName           = "system_metric"
	DataType             = "data"
	MetricInterval       = 1 * time.Second
	ReconnectDelay       = 5 * time.Second
	MaxSendTimeout       = 3 * time.Second
	MaxPendingOps        = 5
	CounterResetAt       = 1000000
	MaxReconnectAttempts = 10
)

// SystemMetrics represents the streamlined data structure for RAW metrics.
type SystemMetrics struct {
	CPUTemperature  float64 `json:"cpuTemperature"`
	CPUPercent      float64 `json:"cpuPercent"`
	DiskPercentUsed float64 `json:"diskPercentUsed"`
	MemPercentUsed  float64 `json:"memPercentUsed"`
	NetBytesSent    uint64  `json:"netBytesSent"`
	NetBytesRecv    uint64  `json:"netBytesRecv"`
	LastBoot        int64   `json:"lastBoot"`  // unix ms
	Timestamp       int64   `json:"timestamp"` // unix ms
}

// SystemMetricsLive represents the full data structure for live metrics.
type SystemMetricsLive struct {
	CPUUsage        float64 `json:"cpu_usage"`
	MemoryUsage     float64 `json:"memory_usage"`
	DiskUsage       float64 `json:"disk_usage"`
	NetworkUsage    string  `json:"network_usage"`
	CPUTemperature  float64 `json:"cpu_temperature"`
	CPUPercent      float64 `json:"cpu_percent"`
	DiskPercentUsed float64 `json:"disk_percent_used"`
	MemPercentUsed  float64 `json:"mem_percent_used"`
	NetBytesSent    uint64  `json:"net_bytes_sent"`
	NetBytesRecv    uint64  `json:"net_bytes_recv"`
	LastBoot        string  `json:"last_boot"`
	Timestamp       int64   `json:"timestamp"`
}

// ServerMessage represents the JSON structure expected by the server
type ServerMessage struct {
	ClientName string      `json:"client_name"`
	DataType   string      `json:"data_type"`
	DataFormat string      `json:"data_format"`
	Timestamp  int64       `json:"timestamp"`
	Payload    interface{} `json:"payload"`
}

// MetricsState holds the state for tracking when to send RAW vs LIVE data
type MetricsState struct {
	mutex         sync.RWMutex
	lastRawSent   time.Time
	rawSentCount  int
	liveSentCount int
	skipCount     int
	errorCount    int
	connected     bool
}

// ClientManager handles the connection and communication with the server
type ClientManager struct {
	socket        zmq.Socket
	serverAddress string
	state         *MetricsState
	socketMutex   sync.Mutex
}

var ServerAddress = "tcp://localhost:5555"

func getCurrentTimestamp() int64 {
	now := time.Now().UTC()
	currentMinute := now.Truncate(time.Minute)
	previousMinute := currentMinute.Add(-time.Minute)
	return previousMinute.UnixMilli()
}

func (ms *MetricsState) isTimeForRawData(currentTime time.Time) bool {
	ms.mutex.RLock()
	defer ms.mutex.RUnlock()

	currentMinute := currentTime.Truncate(time.Minute)
	lastRawMinute := ms.lastRawSent.Truncate(time.Minute)

	return ms.lastRawSent.IsZero() || !currentMinute.Equal(lastRawMinute)
}

func (ms *MetricsState) markRawDataSent(sentTime time.Time) {
	ms.mutex.Lock()
	defer ms.mutex.Unlock()
	ms.lastRawSent = sentTime
	ms.rawSentCount++
}

func (ms *MetricsState) incrementLiveCount() {
	ms.mutex.Lock()
	defer ms.mutex.Unlock()
	ms.liveSentCount++
}

func (ms *MetricsState) incrementSkipCount() {
	ms.mutex.Lock()
	defer ms.mutex.Unlock()
	ms.skipCount++
}

func (ms *MetricsState) incrementErrorCount() {
	ms.mutex.Lock()
	defer ms.mutex.Unlock()
	ms.errorCount++
}

func (ms *MetricsState) setConnected(connected bool) {
	ms.mutex.Lock()
	defer ms.mutex.Unlock()
	oldState := ms.connected
	ms.connected = connected
	if oldState != connected {
		log.Printf("[STATE] Connection state changed: %v -> %v", oldState, connected)
	}
}

func (ms *MetricsState) isConnected() bool {
	ms.mutex.RLock()
	defer ms.mutex.RUnlock()
	return ms.connected
}

func (ms *MetricsState) getStats() (int, int, int, int) {
	ms.mutex.RLock()
	defer ms.mutex.RUnlock()
	return ms.rawSentCount, ms.liveSentCount, ms.skipCount, ms.errorCount
}

func (ms *MetricsState) resetCountersIfNeeded() {
	ms.mutex.Lock()
	defer ms.mutex.Unlock()

	if ms.rawSentCount >= CounterResetAt || ms.liveSentCount >= CounterResetAt {
		log.Printf("[COUNTER] Resetting counters to prevent overflow")
		ms.rawSentCount = 0
		ms.liveSentCount = 0
		ms.skipCount = 0
		ms.errorCount = 0
	}
}

func NewClientManager(serverAddr string) *ClientManager {
	return &ClientManager{
		serverAddress: serverAddr,
		state:         &MetricsState{},
	}
}

func (cm *ClientManager) Connect() error {
	cm.socketMutex.Lock()
	defer cm.socketMutex.Unlock()

	// Close existing socket if any
	if cm.socket != nil {
		cm.socket.Close()
	}

	// Create new socket with context
	cm.socket = zmq.NewDealer(context.Background())

	log.Printf("[CONNECT] Attempting to connect to %s", cm.serverAddress)
	err := cm.socket.Dial(cm.serverAddress)
	if err != nil {
		return fmt.Errorf("failed to dial server: %w", err)
	}

	// Test connection with a simple ping
	testMessage := []byte(`{"client_name":"` + ClientName + `","data_type":"ping","data_format":"test","timestamp":` + fmt.Sprintf("%d", time.Now().UnixMilli()) + `,"payload":{"status":"connection_test"}}`)

	err = cm.socket.Send(zmq.NewMsg(testMessage))
	if err != nil {
		return fmt.Errorf("failed to send test message: %w", err)
	}

	// Try to receive response with timeout
	replyChan := make(chan zmq.Msg, 1)
	errChan := make(chan error, 1)

	go func() {
		reply, err := cm.socket.Recv()
		if err != nil {
			errChan <- err
		} else {
			replyChan <- reply
		}
	}()

	timer := time.NewTimer(MaxSendTimeout)
	defer timer.Stop()

	select {
	case reply := <-replyChan:
		log.Printf("[CONNECT] Connection test successful - received: %s", string(reply.Bytes()))
		cm.state.setConnected(true)
		return nil
	case err := <-errChan:
		return fmt.Errorf("connection test failed: %w", err)
	case <-timer.C:
		return fmt.Errorf("connection test timed out")
	}
}

func (cm *ClientManager) SendMessage(data []byte) error {
	cm.socketMutex.Lock()
	defer cm.socketMutex.Unlock()

	if !cm.state.isConnected() {
		return fmt.Errorf("not connected to server")
	}

	log.Printf("[SEND] Sending %d bytes to server", len(data))

	// Send message
	sendChan := make(chan error, 1)
	go func() {
		sendChan <- cm.socket.Send(zmq.NewMsg(data))
	}()

	sendTimer := time.NewTimer(MaxSendTimeout)
	defer sendTimer.Stop()

	select {
	case err := <-sendChan:
		if err != nil {
			cm.state.setConnected(false)
			return fmt.Errorf("send failed: %w", err)
		}
	case <-sendTimer.C:
		cm.state.setConnected(false)
		return fmt.Errorf("send timeout")
	}

	// Receive reply
	replyChan := make(chan zmq.Msg, 1)
	errChan := make(chan error, 1)

	go func() {
		reply, err := cm.socket.Recv()
		if err != nil {
			errChan <- err
		} else {
			replyChan <- reply
		}
	}()

	replyTimer := time.NewTimer(MaxSendTimeout)
	defer replyTimer.Stop()

	select {
	case reply := <-replyChan:
		log.Printf("[RECV] Received response: %s", string(reply.Bytes()))
		return nil
	case err := <-errChan:
		cm.state.setConnected(false)
		return fmt.Errorf("receive failed: %w", err)
	case <-replyTimer.C:
		cm.state.setConnected(false)
		return fmt.Errorf("receive timeout")
	}
}

func (cm *ClientManager) Close() {
	cm.socketMutex.Lock()
	defer cm.socketMutex.Unlock()
	if cm.socket != nil {
		cm.socket.Close()
	}
}

func CollectSystemStats() (cpuUsage []float64, memStats *mem.VirtualMemoryStat, diskStats *disk.UsageStat, netStats []net.IOCountersStat, hostStat *host.InfoStat, err error) {
	cpuUsage, err = cpu.Percent(0, false)
	if err != nil {
		return nil, nil, nil, nil, nil, fmt.Errorf("failed to collect CPU usage: %w", err)
	}

	memStats, err = mem.VirtualMemory()
	if err != nil {
		return nil, nil, nil, nil, nil, fmt.Errorf("failed to collect memory usage: %w", err)
	}

	diskStats, err = disk.Usage("/")
	if err != nil {
		return nil, nil, nil, nil, nil, fmt.Errorf("failed to collect disk usage: %w", err)
	}

	netStats, err = net.IOCounters(false)
	if err != nil || len(netStats) == 0 {
		return nil, nil, nil, nil, nil, fmt.Errorf("failed to collect network stats: %w", err)
	}

	hostStat, err = host.Info()
	if err != nil {
		return nil, nil, nil, nil, nil, fmt.Errorf("failed to get system info: %w", err)
	}

	return cpuUsage, memStats, diskStats, netStats, hostStat, nil
}

func CreateSystemMetrics(cpuUsage []float64, memStats *mem.VirtualMemoryStat, diskStats *disk.UsageStat, netStats []net.IOCountersStat, hostStat *host.InfoStat) *SystemMetrics {
	return &SystemMetrics{
		CPUTemperature:  50,
		CPUPercent:      cpuUsage[0],
		DiskPercentUsed: diskStats.UsedPercent,
		MemPercentUsed:  memStats.UsedPercent,
		NetBytesSent:    netStats[0].BytesSent,
		NetBytesRecv:    netStats[0].BytesRecv,
		LastBoot:        int64(hostStat.BootTime) * 1000,
		Timestamp:       getCurrentTimestamp(),
	}
}

func ConvertToLiveMetrics(rawMetrics *SystemMetrics, memStats *mem.VirtualMemoryStat, diskStats *disk.UsageStat, netStats []net.IOCountersStat) *SystemMetricsLive {
	lastBootTime := time.Unix(rawMetrics.LastBoot/1000, 0)

	return &SystemMetricsLive{
		CPUUsage:        rawMetrics.CPUPercent,
		MemoryUsage:     rawMetrics.MemPercentUsed,
		DiskUsage:       rawMetrics.DiskPercentUsed,
		NetworkUsage:    fmt.Sprintf("Bytes Sent: %d, Bytes Received: %d", rawMetrics.NetBytesSent, rawMetrics.NetBytesRecv),
		CPUTemperature:  rawMetrics.CPUTemperature,
		CPUPercent:      rawMetrics.CPUPercent,
		DiskPercentUsed: rawMetrics.DiskPercentUsed,
		MemPercentUsed:  rawMetrics.MemPercentUsed,
		NetBytesSent:    rawMetrics.NetBytesSent,
		NetBytesRecv:    rawMetrics.NetBytesRecv,
		LastBoot:        lastBootTime.Format(time.RFC3339),
		Timestamp:       rawMetrics.Timestamp,
	}
}

func createJSONMessage(clientName, dataType, dataFormat string, payload []byte) ([]byte, error) {
	var metricsData interface{}
	if err := json.Unmarshal(payload, &metricsData); err != nil {
		return nil, fmt.Errorf("failed to parse metrics payload: %w", err)
	}

	timestamp := getCurrentTimestamp()
	serverMsg := ServerMessage{
		ClientName: clientName,
		DataType:   dataType,
		DataFormat: dataFormat,
		Timestamp:  timestamp,
		Payload:    metricsData,
	}

	jsonBytes, err := json.Marshal(serverMsg)
	if err != nil {
		return nil, fmt.Errorf("failed to marshal server message: %w", err)
	}

	return jsonBytes, nil
}

func (cm *ClientManager) sendRawData(rawMetrics *SystemMetrics, currentTime time.Time) error {
	log.Printf("[FORMAT] Sending RAW data for minute boundary at %s", currentTime.Format("15:04:05"))

	rawPayload, err := json.Marshal(rawMetrics)
	if err != nil {
		return fmt.Errorf("error marshaling RAW metrics to JSON: %w", err)
	}

	rawMessage, err := createJSONMessage(ClientName, DataType, "raw", rawPayload)
	if err != nil {
		return fmt.Errorf("error creating RAW JSON message: %w", err)
	}

	err = cm.SendMessage(rawMessage)
	if err != nil {
		return fmt.Errorf("error sending RAW metrics: %w", err)
	}

	cm.state.markRawDataSent(currentTime)
	rawCount, _, _, _ := cm.state.getStats()
	log.Printf("[CYCLE] RAW metrics sent successfully for minute %d (total RAW sent: %d)",
		currentTime.Minute(), rawCount)

	return nil
}

func (cm *ClientManager) sendLiveData(rawMetrics *SystemMetrics, memStats *mem.VirtualMemoryStat, diskStats *disk.UsageStat, netStats []net.IOCountersStat) error {
	log.Printf("[FORMAT] Sending LIVE data for real-time caching")

	liveMetrics := ConvertToLiveMetrics(rawMetrics, memStats, diskStats, netStats)
	livePayload, err := json.Marshal(liveMetrics)
	if err != nil {
		return fmt.Errorf("error marshaling LIVE metrics to JSON: %w", err)
	}

	liveMessage, err := createJSONMessage(ClientName, DataType, "live", livePayload)
	if err != nil {
		return fmt.Errorf("error creating LIVE JSON message: %w", err)
	}

	err = cm.SendMessage(liveMessage)
	if err != nil {
		return fmt.Errorf("error sending LIVE metrics: %w", err)
	}

	cm.state.incrementLiveCount()
	_, liveCount, _, _ := cm.state.getStats()
	log.Printf("[CYCLE] LIVE metrics sent successfully (total LIVE sent: %d)", liveCount)

	return nil
}

func (cm *ClientManager) RunMetricsLoop() {
	ticker := time.NewTicker(MetricInterval)
	defer ticker.Stop()

	reconnectAttempts := 0

	for range ticker.C {
		// Check connection and reconnect if needed
		if !cm.state.isConnected() {
			if reconnectAttempts >= MaxReconnectAttempts {
				log.Printf("[ERROR] Max reconnect attempts (%d) reached, stopping", MaxReconnectAttempts)
				return
			}

			reconnectAttempts++
			log.Printf("[RECONNECT] Attempting to reconnect (%d/%d)", reconnectAttempts, MaxReconnectAttempts)

			err := cm.Connect()
			if err != nil {
				log.Printf("[RECONNECT] Connection failed: %v", err)
				cm.state.incrementSkipCount()
				continue
			}

			log.Printf("[RECONNECT] Successfully reconnected!")
			reconnectAttempts = 0 // Reset on successful connection
		}

		// Handle counter overflow for statistics
		cm.state.resetCountersIfNeeded()

		currentTime := time.Now()
		needsRawData := cm.state.isTimeForRawData(currentTime)

		var cycleType string
		if needsRawData {
			cycleType = "RAW + LIVE"
		} else {
			cycleType = "LIVE"
		}

		rawCount, liveCount, skipCount, errorCount := cm.state.getStats()
		log.Printf("\n[CYCLE] Starting metrics collection (%s) at %s", cycleType, currentTime.Format("15:04:05"))
		log.Printf("[STATS] Sent: RAW=%d, LIVE=%d, Skipped=%d, Errors=%d", rawCount, liveCount, skipCount, errorCount)

		// Collect all system stats first
		log.Println("[METRICS] Starting system metrics collection")
		cpuUsage, memStats, diskStats, netStats, hostStat, err := CollectSystemStats()
		if err != nil {
			log.Printf("[CYCLE] Error collecting system stats: %v", err)
			cm.state.incrementErrorCount()
			continue
		}

		log.Printf("[METRICS] CPU: %.2f%%, Memory: %.2f%%, Disk: %.2f%%",
			cpuUsage[0], memStats.UsedPercent, diskStats.UsedPercent)

		// Create metrics objects
		rawMetrics := CreateSystemMetrics(cpuUsage, memStats, diskStats, netStats, hostStat)

		// Send RAW data first if needed (higher priority)
		if needsRawData {
			err := cm.sendRawData(rawMetrics, currentTime)
			if err != nil {
				log.Printf("[CYCLE] %v", err)
				cm.state.incrementErrorCount()
				continue
			}
		}

		// Always send LIVE data
		err = cm.sendLiveData(rawMetrics, memStats, diskStats, netStats)
		if err != nil {
			log.Printf("[CYCLE] %v", err)
			cm.state.incrementErrorCount()
			continue
		}

		log.Printf("[SUMMARY] CPU: %.1f%%, Memory: %.1f%%, Disk: %.1f%%",
			rawMetrics.CPUPercent, rawMetrics.MemPercentUsed, rawMetrics.DiskPercentUsed)
		log.Printf("[CYCLE] End of metrics collection (%s)\n", cycleType)
	}
}

func main() {
	log.Println("[STARTUP] Starting System Metrics Client - Simplified Single-Threaded Version")
	log.Printf("[CONFIG] Configuration:")
	log.Printf("[CONFIG] Server Address: %s", ServerAddress)
	log.Printf("[CONFIG] Client Name: %s", ClientName)
	log.Printf("[CONFIG] Data Type: %s", DataType)
	log.Printf("[CONFIG] Metric Interval: %v", MetricInterval)
	log.Printf("[CONFIG] Reconnect Delay: %v", ReconnectDelay)
	log.Printf("[CONFIG] Max Send Timeout: %v", MaxSendTimeout)
	log.Printf("[CONFIG] Max Reconnect Attempts: %d", MaxReconnectAttempts)
	log.Printf("[CONFIG] Mode: RAW (beginning of minute) + LIVE (every second)")

	// Create client manager
	client := NewClientManager(ServerAddress)
	defer client.Close()

	// Initial connection
	log.Println("[STARTUP] Attempting initial connection...")
	err := client.Connect()
	if err != nil {
		log.Printf("[STARTUP] Initial connection failed: %v", err)
		log.Println("[STARTUP] Will retry during metrics loop")
	} else {
		log.Println("[STARTUP] Initial connection successful")
	}

	log.Println("[STARTUP] Starting metrics collection loop")
	client.RunMetricsLoop()
}
