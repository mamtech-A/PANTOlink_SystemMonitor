#include "RAMMonitor.hpp"

#include <algorithm>
#include <cctype>
#include <dirent.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

void RAMMonitor::sample() {
    long totalMemoryKb = getTotalMemoryKb();
    long memAvailableKb = 0;

    std::ifstream meminfo("/proc/meminfo");
    std::string key;
    long value = 0;
    std::string unit;

    while (meminfo >> key >> value >> unit) {
        if (key == "MemAvailable:") {
            memAvailableKb = value;
            break;
        }
    }

    if (totalMemoryKb > 0) {
        const long usedKb = totalMemoryKb - memAvailableKb;
        last_memory_percent_used_ =
            100.0 * static_cast<double>(usedKb) /
            static_cast<double>(totalMemoryKb);
    } else {
        last_memory_percent_used_ = 0.0;
    }

    last_top_processes_ = getTopRamUsers(10);
}

void RAMMonitor::update() {
    sample();

    std::cout << std::left
              << std::setw(8)  << "PID"
              << std::setw(30) << "Process"
              << std::setw(15) << "RAM (MB)"
              << std::setw(12) << "RAM (%)"
              << '\n';

    std::cout << std::string(65, '-') << '\n';

    for (const auto& process : last_top_processes_) {
        std::cout << std::left
                  << std::setw(8)  << process.pid
                  << std::setw(30) << process.name
                  << std::setw(15) << std::fixed << std::setprecision(2)
                  << (process.memoryKb / 1024.0)
                  << std::setw(12) << std::fixed << std::setprecision(2)
                  << process.memoryPercent
                  << '\n';
    }
}

double RAMMonitor::getMemoryPercentUsed() const {
    return last_memory_percent_used_;
}

std::vector<RAMMonitor::ProcessInfo> RAMMonitor::getTopProcesses() const {
    return last_top_processes_;
}

std::vector<RAMMonitor::ProcessInfo> RAMMonitor::getTopRamUsers(std::size_t topCount) {
    std::vector<ProcessInfo> processes;
    long totalMemoryKb = getTotalMemoryKb();

    DIR* procDir = opendir("/proc");
    if (!procDir) {
        return processes;
    }

    dirent* entry;
    while ((entry = readdir(procDir)) != nullptr) {
        std::string dirName = entry->d_name;

        if (!isNumeric(dirName)) {
            continue;
        }

        int pid = std::stoi(dirName);
        long memoryKb = readProcessMemoryKb(pid);

        if (memoryKb <= 0) {
            continue;
        }

        ProcessInfo process;
        process.pid = pid;
        process.name = readProcessName(pid);
        process.memoryKb = memoryKb;
        process.memoryPercent =
            (static_cast<double>(memoryKb) / totalMemoryKb) * 100.0;

        processes.push_back(process);
    }

    closedir(procDir);

    std::sort(processes.begin(), processes.end(),
              [](const ProcessInfo& a, const ProcessInfo& b) {
                  return a.memoryKb > b.memoryKb;
              });

    if (processes.size() > topCount) {
        processes.resize(topCount);
    }

    return processes;
}

long RAMMonitor::getTotalMemoryKb() {
    std::ifstream meminfo("/proc/meminfo");

    std::string key;
    long value;
    std::string unit;

    while (meminfo >> key >> value >> unit) {
        if (key == "MemTotal:") {
            return value;
        }
    }

    return -1;
}

bool RAMMonitor::isNumeric(const std::string& text) {
    for (char c : text) {
        if (!std::isdigit(c)) {
            return false;
        }
    }
    return !text.empty();
}

std::string RAMMonitor::readProcessName(int pid) {
    std::ifstream file("/proc/" + std::to_string(pid) + "/comm");

    std::string name;
    std::getline(file, name);

    if (name.empty()) {
        name = "Unknown";
    }

    return name;
}

long RAMMonitor::readProcessMemoryKb(int pid) {
    std::ifstream file("/proc/" + std::to_string(pid) + "/status");

    std::string line;
    while (std::getline(file, line)) {
        if (line.rfind("VmRSS:", 0) == 0) {
            std::istringstream iss(line);
            std::string key;
            long mem;
            std::string unit;

            iss >> key >> mem >> unit;
            return mem;
        }
    }

    return -1;
}