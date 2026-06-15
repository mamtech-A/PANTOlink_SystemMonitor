#ifndef ZMQ_CLIENT_HPP
#define ZMQ_CLIENT_HPP

#include "bootTimeMonitor.hpp"
#include "cpuMonitor.hpp"
#include "diskMonitor.hpp"
#include "internetConnectionMonitor.hpp"
#include "jsonMaker.hpp"
#include "RAMMonitor.hpp"

#include <chrono>
#include <memory>
#include <string>
#include <zmq.hpp>

class ZMQClient {
public:
    ZMQClient(CPUMonitor& cpuMonitor,
              RAMMonitor& ramMonitor,
              DiskMonitor& diskMonitor,
              InternetConnectionMonitor& internetMonitor,
              BootTimeMonitor& bootTimeMonitor,
              const std::string& serverAddress = "tcp://localhost:5555");

    ~ZMQClient();

    bool connect();
    bool sendMessage(const std::string& data);
    void close();

    void runMetricsLoop();

private:
    bool sendPingTest();
    bool sendFullData();
    bool sendLiveData();
    bool isTimeForFullData(const std::chrono::system_clock::time_point& currentTime) const;

    void sampleAllMonitors();
    void resetCountersIfNeeded();

private:
    static constexpr const char* kClientName = "system_metric";
    static constexpr const char* kDataType = "data";
    static constexpr int kMetricIntervalMs = 1000;
    static constexpr int kReconnectDelayMs = 5000;
    static constexpr int kMaxSendTimeoutMs = 3000;
    static constexpr int kCounterResetAt = 1000000;
    static constexpr int kMaxReconnectAttempts = 10;

private:
    CPUMonitor& cpuMonitor_;
    RAMMonitor& ramMonitor_;
    DiskMonitor& diskMonitor_;
    InternetConnectionMonitor& internetMonitor_;
    BootTimeMonitor& bootTimeMonitor_;
    JSONMaker jsonMaker_;

    zmq::context_t context_;
    std::unique_ptr<zmq::socket_t> socket_;
    std::string serverAddress_;

    bool connected_;
    std::chrono::system_clock::time_point lastFullSent_;

    int fullSentCount_;
    int liveSentCount_;
    int skipCount_;
    int errorCount_;
};

#endif