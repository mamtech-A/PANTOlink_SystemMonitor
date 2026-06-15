#include "cpuMonitor.hpp"

#include <algorithm>
#include <cctype>
#include <dirent.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <unistd.h>

long CPUMonitor::CPUData::totalTime() const {
    return user + nice + system + idle + iowait + irq + softirq + steal;
}

long CPUMonitor::CPUData::idleTime() const {
    return idle + iowait;
}

CPUMonitor::CPUMonitor()
    : core_count_(detectCoreCount()),
      previous_cpu_data_(readCPUData()),
      previous_processes_(readProcesses()) {
}

void CPUMonitor::sample() {
    const auto current_cpu_data = readCPUData();

    if (previous_cpu_data_.empty() || current_cpu_data.empty()) {
        last_total_cpu_usage_ = 0.0;
        last_cpu_temperature_ = 0.0;
        last_core_usages_.clear();
        last_load_averages_.clear();
        last_top_processes_.clear();
        return;
    }

    refreshStoredCPUValues(current_cpu_data);
    refreshStoredTopProcesses();

    previous_cpu_data_ = current_cpu_data;
}

void CPUMonitor::update() {
    const auto current_cpu_data = readCPUData();

    if (previous_cpu_data_.empty() || current_cpu_data.empty()) {
        std::cerr << "Failed to read CPU data\n";
        return;
    }

    refreshStoredCPUValues(current_cpu_data);
    refreshStoredTopProcesses();

    printSummary(current_cpu_data);
    printTopProcesses();

    previous_cpu_data_ = current_cpu_data;
}

int CPUMonitor::getCoreCount() const {
    return core_count_;
}

double CPUMonitor::getTotalCPUUsage() const {
    return last_total_cpu_usage_;
}

double CPUMonitor::getCPUTemperature() const {
    return last_cpu_temperature_;
}

std::vector<double> CPUMonitor::getCoreUsages() const {
    return last_core_usages_;
}

std::vector<double> CPUMonitor::getLoadAverages() const {
    return last_load_averages_;
}

std::vector<CPUMonitor::ProcessData> CPUMonitor::getTopCPUProcesses() const {
    return last_top_processes_;
}

int CPUMonitor::detectCoreCount() const {
    const unsigned int count = std::thread::hardware_concurrency();
    return (count == 0) ? 1 : static_cast<int>(count);
}

std::vector<CPUMonitor::CPUData> CPUMonitor::readCPUData() const {
    std::vector<CPUData> cpu_data_list;
    std::ifstream stat_file("/proc/stat");

    if (!stat_file.is_open()) {
        return cpu_data_list;
    }

    std::string line;
    while (std::getline(stat_file, line)) {
        if (line.rfind("cpu", 0) != 0) {
            continue;
        }

        std::istringstream stream(line);
        CPUData cpu;

        stream >> cpu.name
               >> cpu.user
               >> cpu.nice
               >> cpu.system
               >> cpu.idle
               >> cpu.iowait
               >> cpu.irq
               >> cpu.softirq
               >> cpu.steal;

        if (!stream.fail()) {
            cpu_data_list.push_back(cpu);
        }

        if (static_cast<int>(cpu_data_list.size()) > core_count_) {
            break;
        }
    }

    return cpu_data_list;
}

double CPUMonitor::calculateCPUUsage(const CPUData& previous, const CPUData& current) const {
    const long total_diff = current.totalTime() - previous.totalTime();
    const long idle_diff = current.idleTime() - previous.idleTime();

    if (total_diff <= 0) {
        return 0.0;
    }

    return 100.0 * static_cast<double>(total_diff - idle_diff) /
           static_cast<double>(total_diff);
}

std::vector<double> CPUMonitor::readLoadAverage() const {
    std::vector<double> loads(3, 0.0);
    std::ifstream load_file("/proc/loadavg");

    if (load_file.is_open()) {
        load_file >> loads[0] >> loads[1] >> loads[2];
    }

    return loads;
}

double CPUMonitor::readCPUTemperature() const {
    const std::vector<std::string> paths = {
        "/sys/class/thermal/thermal_zone0/temp",
        "/sys/class/thermal/thermal_zone1/temp",
        "/sys/class/hwmon/hwmon0/temp1_input",
        "/sys/class/hwmon/hwmon1/temp1_input"
    };

    for (const auto& path : paths) {
        std::ifstream file(path);
        if (!file.is_open()) {
            continue;
        }

        int raw_temp = 0;
        file >> raw_temp;

        if (file.fail()) {
            continue;
        }

        const double temp = raw_temp / 1000.0;
        if (temp > 0.0) {
            return temp;
        }
    }

    return 0.0;
}

std::vector<CPUMonitor::ProcessData> CPUMonitor::readProcesses() const {
    std::vector<ProcessData> processes;

    DIR* proc_dir = opendir("/proc");
    if (proc_dir == nullptr) {
        return processes;
    }

    dirent* entry = nullptr;
    while ((entry = readdir(proc_dir)) != nullptr) {
        if (!std::isdigit(entry->d_name[0])) {
            continue;
        }

        const int pid = std::stoi(entry->d_name);
        const std::string stat_path = std::string("/proc/") + entry->d_name + "/stat";
        std::ifstream stat_file(stat_path);

        if (!stat_file.is_open()) {
            continue;
        }

        std::string line;
        std::getline(stat_file, line);
        if (line.empty()) {
            continue;
        }

        const std::size_t open_paren = line.find('(');
        const std::size_t close_paren = line.rfind(')');
        if (open_paren == std::string::npos || close_paren == std::string::npos || close_paren <= open_paren) {
            continue;
        }

        ProcessData process;
        process.pid = pid;
        process.name = line.substr(open_paren + 1, close_paren - open_paren - 1);

        std::istringstream stream(line.substr(close_paren + 2));

        char state{};
        long dummy_long{};
        unsigned long dummy_ulong{};

        stream >> state;
        stream >> dummy_long;
        stream >> dummy_long;
        stream >> dummy_long;
        stream >> dummy_long;
        stream >> dummy_long;
        stream >> dummy_ulong;
        stream >> dummy_ulong;
        stream >> dummy_ulong;
        stream >> dummy_ulong;
        stream >> dummy_ulong;
        stream >> process.utime;
        stream >> process.stime;

        if (stream.fail()) {
            continue;
        }

        process.totalTime = process.utime + process.stime;
        process.cpuUsage = 0.0;

        processes.push_back(process);
    }

    closedir(proc_dir);
    return processes;
}

void CPUMonitor::refreshStoredCPUValues(const std::vector<CPUData>& current_cpu_data) {
    last_core_usages_.clear();

    if (previous_cpu_data_.size() == current_cpu_data.size() && !current_cpu_data.empty()) {
        last_total_cpu_usage_ =
            calculateCPUUsage(previous_cpu_data_[0], current_cpu_data[0]);

        for (int i = 1; i <= core_count_ && i < static_cast<int>(current_cpu_data.size()); ++i) {
            last_core_usages_.push_back(
                calculateCPUUsage(previous_cpu_data_[i], current_cpu_data[i]));
        }
    } else {
        last_total_cpu_usage_ = 0.0;
    }

    last_load_averages_ = readLoadAverage();
    last_cpu_temperature_ = readCPUTemperature();
}

void CPUMonitor::refreshStoredTopProcesses() {
    auto current_processes = readProcesses();
    const long clock_ticks = sysconf(_SC_CLK_TCK);

    for (auto& current : current_processes) {
        for (const auto& previous : previous_processes_) {
            if (current.pid == previous.pid) {
                const long time_diff = current.totalTime - previous.totalTime;
                if (time_diff > 0) {
                    current.cpuUsage = 100.0 * static_cast<double>(time_diff) /
                                       static_cast<double>(clock_ticks);
                }
                break;
            }
        }
    }

    std::sort(
        current_processes.begin(),
        current_processes.end(),
        [](const ProcessData& left, const ProcessData& right) {
            return left.cpuUsage > right.cpuUsage;
        });

    const std::size_t max_count = std::min<std::size_t>(10, current_processes.size());
    last_top_processes_.clear();
    for (std::size_t i = 0; i < max_count; ++i) {
        last_top_processes_.push_back(current_processes[i]);
    }

    previous_processes_ = current_processes;
}

void CPUMonitor::printSummary(const std::vector<CPUData>& current_cpu_data) const {
    std::cout << "Detected " << core_count_ << " CPU cores\n";

    if (previous_cpu_data_.size() != current_cpu_data.size()) {
        std::cerr << "CPU data size mismatch\n";
        return;
    }

    const double total_usage =
        calculateCPUUsage(previous_cpu_data_[0], current_cpu_data[0]);

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Total CPU Usage: " << total_usage << "%\n";

    for (int i = 1; i <= core_count_ && i < static_cast<int>(current_cpu_data.size()); ++i) {
        const double usage =
            calculateCPUUsage(previous_cpu_data_[i], current_cpu_data[i]);

        std::cout << "Core " << (i - 1) << " Usage: " << usage << "%\n";
    }

    const auto loads = readLoadAverage();
    std::cout << "Load Average: "
              << loads[0] << ", "
              << loads[1] << ", "
              << loads[2] << '\n';

    const double temperature = readCPUTemperature();
    if (temperature > 0.0) {
        std::cout << "CPU Temperature: " << temperature << " C\n";
    } else {
        std::cout << "CPU Temperature: Not available\n";
    }

    std::cout << "----------------------------------------\n";
}

void CPUMonitor::printTopProcesses() {
    std::cout << "Top 10 CPU Processes\n";
    std::cout << std::left
              << std::setw(10) << "PID"
              << std::setw(12) << "CPU %"
              << "NAME\n";

    for (const auto& process : last_top_processes_) {
        std::cout << std::left
                  << std::setw(10) << process.pid
                  << std::setw(12) << std::fixed << std::setprecision(2) << process.cpuUsage
                  << process.name << '\n';
    }

    std::cout << "========================================\n";
}