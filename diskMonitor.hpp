#ifndef DISK_MONITOR_HPP
#define DISK_MONITOR_HPP

#include <string>
#include <vector>

class DiskMonitor {
public:
    struct DiskInfo {
        std::string path;
        unsigned long long totalBytes{};
        unsigned long long freeBytes{};
        unsigned long long availableBytes{};
    };

    void update();
    void sample();

    double getDiskPercentUsed() const;
    std::vector<DiskInfo> getLastDisks() const;

private:
    std::vector<DiskInfo> GetDiskInfo(const std::vector<std::string>& paths);
    void PrintDiskInfo(const std::vector<DiskInfo>& disks);
    std::string FormatBytes(unsigned long long bytes);

private:
    double last_disk_percent_used_{0.0};
    std::vector<DiskInfo> last_disks_;
};

#endif