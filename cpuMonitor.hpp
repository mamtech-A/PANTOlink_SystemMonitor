#ifndef CPU_MONITOR_HPP
#define CPU_MONITOR_HPP

#include <string>
#include <vector>

class CPUMonitor {
public:
    struct ProcessData {
        int pid{};
        std::string name;
        long utime{};
        long stime{};
        long totalTime{};
        double cpuUsage{};
    };

    CPUMonitor();

    void update();
    void sample();

    int getCoreCount() const;
    double getTotalCPUUsage() const;
    double getCPUTemperature() const;
    std::vector<double> getCoreUsages() const;
    std::vector<double> getLoadAverages() const;
    std::vector<ProcessData> getTopCPUProcesses() const;

private:
    struct CPUData {
        std::string name;
        long user{};
        long nice{};
        long system{};
        long idle{};
        long iowait{};
        long irq{};
        long softirq{};
        long steal{};

        long totalTime() const;
        long idleTime() const;
    };

    int detectCoreCount() const;
    std::vector<CPUData> readCPUData() const;
    double calculateCPUUsage(const CPUData& previous, const CPUData& current) const;
    std::vector<double> readLoadAverage() const;
    double readCPUTemperature() const;
    std::vector<ProcessData> readProcesses() const;

    void printSummary(const std::vector<CPUData>& current_cpu_data) const;
    void printTopProcesses();

    void refreshStoredCPUValues(const std::vector<CPUData>& current_cpu_data);
    void refreshStoredTopProcesses();

private:
    int core_count_{};
    std::vector<CPUData> previous_cpu_data_;
    std::vector<ProcessData> previous_processes_;

    double last_total_cpu_usage_{0.0};
    double last_cpu_temperature_{0.0};
    std::vector<double> last_core_usages_;
    std::vector<double> last_load_averages_;
    std::vector<ProcessData> last_top_processes_;
};

#endif