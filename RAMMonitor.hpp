#ifndef RAMMONITOR_HPP
#define RAMMONITOR_HPP

#include <string>
#include <vector>

class RAMMonitor {
public:
    struct ProcessInfo {
        int pid{};
        std::string name;
        long memoryKb{};
        double memoryPercent{};
    };

    void update();
    void sample();

    double getMemoryPercentUsed() const;
    std::vector<ProcessInfo> getTopProcesses() const;

private:
    std::vector<ProcessInfo> getTopRamUsers(std::size_t topCount = 10);
    long getTotalMemoryKb();
    bool isNumeric(const std::string& text);
    std::string readProcessName(int pid);
    long readProcessMemoryKb(int pid);

private:
    double last_memory_percent_used_{0.0};
    std::vector<ProcessInfo> last_top_processes_;
};

#endif