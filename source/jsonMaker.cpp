#include "jsonMaker.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {
constexpr const char* kClientName = "system_metric";
constexpr const char* kDataType = "data";
constexpr long long kOneMinuteMs = 60LL * 1000LL;
constexpr long long kLastFourDigitsMask = 10'000LL;
}

JSONMaker::JSONMaker(CPUMonitor& cpuMonitor,
                     RAMMonitor& ramMonitor,
                     DiskMonitor& diskMonitor,
                     InternetConnectionMonitor& internetMonitor,
                     BootTimeMonitor& bootTimeMonitor)
    : cpuMonitor_(cpuMonitor),
      ramMonitor_(ramMonitor),
      diskMonitor_(diskMonitor),
      internetMonitor_(internetMonitor),
      bootTimeMonitor_(bootTimeMonitor) {
}

/* =========================
   FULL JSON
   ========================= */
std::string JSONMaker::makeFullJson() const {
    std::ostringstream out;

    const auto coreUsages = cpuMonitor_.getCoreUsages();
    const auto loadAverages = cpuMonitor_.getLoadAverages();
    const auto topCpuProcesses = cpuMonitor_.getTopCPUProcesses();
    const auto topRamProcesses = ramMonitor_.getTopProcesses();
    const auto disks = diskMonitor_.getLastDisks();

    out << "{";
    out << "\"client_name\":\"" << kClientName << "\",";
    out << "\"data_type\":\"" << kDataType << "\",";
    out << "\"data_format\":\"full\",";
    out << "\"timestamp\":" << getCurrentTimestampMs() << ",";
    out << "\"payload\":{";

    out << "\"cpu\":{";
    out << "\"core_count\":" << cpuMonitor_.getCoreCount() << ",";
    out << "\"total_usage_percent\":" << formatDouble(cpuMonitor_.getTotalCPUUsage()) << ",";
    out << "\"per_core_usage_percent\":[";
    for (std::size_t i = 0; i < coreUsages.size(); ++i) {
        if (i > 0) {
            out << ",";
        }
        out << formatDouble(coreUsages[i]);
    }
    out << "],";
    out << "\"load_average\":{";
    out << "\"one_min\":" << formatDouble(loadAverages.size() > 0 ? loadAverages[0] : 0.0) << ",";
    out << "\"five_min\":" << formatDouble(loadAverages.size() > 1 ? loadAverages[1] : 0.0) << ",";
    out << "\"fifteen_min\":" << formatDouble(loadAverages.size() > 2 ? loadAverages[2] : 0.0);
    out << "},";
    out << "\"temperature_celsius\":" << formatDouble(cpuMonitor_.getCPUTemperature()) << ",";
    out << "\"top_processes\":[";
    for (std::size_t i = 0; i < topCpuProcesses.size(); ++i) {
        if (i > 0) {
            out << ",";
        }
        out << "{";
        out << "\"pid\":" << topCpuProcesses[i].pid << ",";
        out << "\"name\":\"" << escapeJson(topCpuProcesses[i].name) << "\",";
        out << "\"cpu_percent\":" << formatDouble(topCpuProcesses[i].cpuUsage);
        out << "}";
    }
    out << "]";
    out << "},";

    out << "\"memory\":{";
    out << "\"used_percent\":" << formatDouble(ramMonitor_.getMemoryPercentUsed()) << ",";
    out << "\"top_processes\":[";
    for (std::size_t i = 0; i < topRamProcesses.size(); ++i) {
        if (i > 0) {
            out << ",";
        }
        out << "{";
        out << "\"pid\":" << topRamProcesses[i].pid << ",";
        out << "\"name\":\"" << escapeJson(topRamProcesses[i].name) << "\",";
        out << "\"ram_mb\":" << formatDouble(topRamProcesses[i].memoryKb / 1024.0) << ",";
        out << "\"ram_percent\":" << formatDouble(topRamProcesses[i].memoryPercent);
        out << "}";
    }
    out << "]";
    out << "},";

    out << "\"network\":{";
    out << "\"connected\":" << (internetMonitor_.getLastConnectionState() ? "true" : "false") << ",";
    out << "\"download_bytes\":" << internetMonitor_.getLastRxDiff() << ",";
    out << "\"upload_bytes\":" << internetMonitor_.getLastTxDiff() << ",";
    out << "\"total_rx_bytes\":" << internetMonitor_.getLastRxBytes() << ",";
    out << "\"total_tx_bytes\":" << internetMonitor_.getLastTxBytes();
    out << "},";

    out << "\"disk\":{";
    out << "\"items\":[";
    for (std::size_t i = 0; i < disks.size(); ++i) {
        if (i > 0) {
            out << ",";
        }

        const auto& disk = disks[i];
        const unsigned long long usedBytes = disk.totalBytes - disk.freeBytes;
        const double usedPercent =
            (disk.totalBytes > 0)
                ? 100.0 * static_cast<double>(usedBytes) / static_cast<double>(disk.totalBytes)
                : 0.0;

        out << "{";
        out << "\"path\":\"" << escapeJson(disk.path) << "\",";
        out << "\"total_bytes\":" << disk.totalBytes << ",";
        out << "\"free_bytes\":" << disk.freeBytes << ",";
        out << "\"available_bytes\":" << disk.availableBytes << ",";
        out << "\"used_percent\":" << formatDouble(usedPercent);
        out << "}";
    }
    out << "]";
    out << "},";

    out << "\"system\":{";
    out << "\"last_boot\":\"" << escapeJson(bootTimeMonitor_.getLastBootTimeString()) << "\",";
    out << "\"uptime_seconds\":" << formatDouble(bootTimeMonitor_.readUptimeSeconds());
    out << "}";

    out << "}";
    out << "}";

    return out.str();
}

/* =========================
   GO STYLE
   ========================= */
std::string JSONMaker::makeGoPingJson() const {
    std::ostringstream out;
    out << "{";
    out << "\"client_name\":\"" << kClientName << "\",";
    out << "\"data_type\":\"ping\",";
    out << "\"data_format\":\"test\",";
    out << "\"timestamp\":" << getCurrentTimestampMs() << ",";
    out << "\"payload\":{\"status\":\"connection_test\"}";
    out << "}";
    return out.str();
}

std::string JSONMaker::makeGoLiveJson() const {
    const long long ts = getLiveGoTimestampMs();

    std::ostringstream out;
    out << "{";
    out << "\"client_name\":\"" << kClientName << "\",";
    out << "\"data_type\":\"data\",";
    out << "\"data_format\":\"live\",";
    out << "\"timestamp\":" << ts << ",";
    out << "\"payload\":{";

    out << "\"cpu\":" << formatDouble(cpuMonitor_.getTotalCPUUsage()) << ",";
    out << "\"mem\":" << formatDouble(ramMonitor_.getMemoryPercentUsed()) << ",";
    out << "\"disk\":" << formatDouble(diskMonitor_.getDiskPercentUsed()) << ",";
    out << "\"timestamp\":" << ts;

    out << "}}";
    return out.str();
}

std::string JSONMaker::makeGoFullJson() const {
    const long long ts = getFullGoTimestampMs();
    std::ostringstream out;

    const auto coreUsages = cpuMonitor_.getCoreUsages();
    const auto loadAverages = cpuMonitor_.getLoadAverages();
    const auto topCpuProcesses = cpuMonitor_.getTopCPUProcesses();
    const auto topRamProcesses = ramMonitor_.getTopProcesses();
    const auto disks = diskMonitor_.getLastDisks();

    out << "{";
    out << "\"client_name\":\"" << kClientName << "\",";
    out << "\"data_type\":\"data\",";
    out << "\"data_format\":\"full\",";
    out << "\"timestamp\":" << ts << ",";
    out << "\"payload\":{";

    out << "\"cpu\":{";
    out << "\"core_count\":" << cpuMonitor_.getCoreCount() << ",";
    out << "\"total_usage_percent\":" << formatDouble(cpuMonitor_.getTotalCPUUsage()) << ",";
    out << "\"per_core_usage_percent\":[";
    for (std::size_t i = 0; i < coreUsages.size(); ++i) {
        if (i > 0) {
            out << ",";
        }
        out << formatDouble(coreUsages[i]);
    }
    out << "],";
    out << "\"load_average\":{";
    out << "\"one_min\":" << formatDouble(loadAverages.size() > 0 ? loadAverages[0] : 0.0) << ",";
    out << "\"five_min\":" << formatDouble(loadAverages.size() > 1 ? loadAverages[1] : 0.0) << ",";
    out << "\"fifteen_min\":" << formatDouble(loadAverages.size() > 2 ? loadAverages[2] : 0.0);
    out << "},";
    out << "\"temperature_celsius\":" << formatDouble(cpuMonitor_.getCPUTemperature()) << ",";
    out << "\"top_processes\":[";
    for (std::size_t i = 0; i < topCpuProcesses.size(); ++i) {
        if (i > 0) {
            out << ",";
        }
        out << "{";
        out << "\"pid\":" << topCpuProcesses[i].pid << ",";
        out << "\"name\":\"" << escapeJson(topCpuProcesses[i].name) << "\",";
        out << "\"cpu_percent\":" << formatDouble(topCpuProcesses[i].cpuUsage);
        out << "}";
    }
    out << "]";
    out << "},";

    out << "\"memory\":{";
    out << "\"used_percent\":" << formatDouble(ramMonitor_.getMemoryPercentUsed()) << ",";
    out << "\"top_processes\":[";
    for (std::size_t i = 0; i < topRamProcesses.size(); ++i) {
        if (i > 0) {
            out << ",";
        }
        out << "{";
        out << "\"pid\":" << topRamProcesses[i].pid << ",";
        out << "\"name\":\"" << escapeJson(topRamProcesses[i].name) << "\",";
        out << "\"ram_mb\":" << formatDouble(topRamProcesses[i].memoryKb / 1024.0) << ",";
        out << "\"ram_percent\":" << formatDouble(topRamProcesses[i].memoryPercent);
        out << "}";
    }
    out << "]";
    out << "},";

    out << "\"network\":{";
    out << "\"connected\":" << (internetMonitor_.getLastConnectionState() ? "true" : "false") << ",";
    out << "\"download_bytes\":" << internetMonitor_.getLastRxDiff() << ",";
    out << "\"upload_bytes\":" << internetMonitor_.getLastTxDiff() << ",";
    out << "\"total_rx_bytes\":" << internetMonitor_.getLastRxBytes() << ",";
    out << "\"total_tx_bytes\":" << internetMonitor_.getLastTxBytes();
    out << "},";

    out << "\"disk\":{";
    out << "\"items\":[";
    for (std::size_t i = 0; i < disks.size(); ++i) {
        if (i > 0) {
            out << ",";
        }

        const auto& disk = disks[i];
        const unsigned long long usedBytes = disk.totalBytes - disk.freeBytes;
        const double usedPercent =
            (disk.totalBytes > 0)
                ? 100.0 * static_cast<double>(usedBytes) / static_cast<double>(disk.totalBytes)
                : 0.0;

        out << "{";
        out << "\"path\":\"" << escapeJson(disk.path) << "\",";
        out << "\"total_bytes\":" << disk.totalBytes << ",";
        out << "\"free_bytes\":" << disk.freeBytes << ",";
        out << "\"available_bytes\":" << disk.availableBytes << ",";
        out << "\"used_percent\":" << formatDouble(usedPercent);
        out << "}";
    }
    out << "]";
    out << "},";

    out << "\"system\":{";
    out << "\"last_boot\":\"" << escapeJson(bootTimeMonitor_.getLastBootTimeString()) << "\",";
    out << "\"uptime_seconds\":" << formatDouble(bootTimeMonitor_.readUptimeSeconds());
    out << "}";

    out << "}";
    out << "}";

    return out.str();
}

/* =========================
   PRINT
   ========================= */
void JSONMaker::printGoPingJson() const {
    std::cout << makeGoPingJson() << '\n';
}

void JSONMaker::printGoLiveJson() const {
    std::cout << makeGoLiveJson() << '\n';
}

void JSONMaker::printGoFullJson() const {
    std::cout << makeGoFullJson() << '\n';
}

void JSONMaker::printFullJson() const {
    std::cout << makeFullJson() << '\n';
}

/* =========================
   UTILS
   ========================= */
std::string JSONMaker::escapeJson(const std::string& text) const {
    std::ostringstream out;
    for (char ch : text) {
        if (ch == '\"') {
            out << "\\\"";
        } else if (ch == '\\') {
            out << "\\\\";
        } else {
            out << ch;
        }
    }
    return out.str();
}

std::string JSONMaker::formatDouble(double value, int precision) const {
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

/* =========================
   TIMESTAMP LOGIC
   ========================= */
long long JSONMaker::getLiveGoTimestampMs() const {
    const auto now = std::chrono::system_clock::now();
    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now.time_since_epoch())
                       .count();
    return ms - kOneMinuteMs;
}

long long JSONMaker::getFullGoTimestampMs() const {
    const auto now = std::chrono::system_clock::now();
    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now.time_since_epoch())
                       .count();
    ms -= kOneMinuteMs;
    return (ms / kLastFourDigitsMask) * kLastFourDigitsMask;
}

long long JSONMaker::getCurrentTimestampMs() const {
    const auto now = std::chrono::system_clock::now();
    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now.time_since_epoch())
                       .count();

    ms -= kOneMinuteMs;
    return (ms / kLastFourDigitsMask) * kLastFourDigitsMask;
}