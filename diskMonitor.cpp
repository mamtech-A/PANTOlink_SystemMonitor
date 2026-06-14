#include "diskMonitor.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <sys/statvfs.h>

void DiskMonitor::sample()
{
    std::vector<std::string> paths = {"/", "/home"};
    last_disks_ = GetDiskInfo(paths);

    last_disk_percent_used_ = 0.0;
    for (const auto& disk : last_disks_) {
        if (disk.path == "/" && disk.totalBytes > 0) {
            const unsigned long long usedBytes = disk.totalBytes - disk.freeBytes;
            last_disk_percent_used_ =
                100.0 * static_cast<double>(usedBytes) /
                static_cast<double>(disk.totalBytes);
            break;
        }
    }
}

void DiskMonitor::update()
{
    sample();
    PrintDiskInfo(last_disks_);
}

double DiskMonitor::getDiskPercentUsed() const
{
    return last_disk_percent_used_;
}

std::vector<DiskMonitor::DiskInfo> DiskMonitor::getLastDisks() const
{
    return last_disks_;
}

std::vector<DiskMonitor::DiskInfo> DiskMonitor::GetDiskInfo(const std::vector<std::string>& paths)
{
    std::vector<DiskInfo> disks;

    for (const std::string& path : paths) {
        struct statvfs stat{};

        if (statvfs(path.c_str(), &stat) == 0) {
            DiskInfo info;

            info.path = path;
            info.totalBytes = static_cast<unsigned long long>(stat.f_blocks) * stat.f_frsize;
            info.freeBytes = static_cast<unsigned long long>(stat.f_bfree) * stat.f_frsize;
            info.availableBytes = static_cast<unsigned long long>(stat.f_bavail) * stat.f_frsize;

            disks.push_back(info);
        }
    }

    return disks;
}

void DiskMonitor::PrintDiskInfo(const std::vector<DiskInfo>& disks)
{
    std::cout << std::left
              << std::setw(15) << "Path"
              << std::setw(15) << "Total"
              << std::setw(15) << "Free"
              << std::setw(15) << "Available"
              << "\n";

    std::cout << "-----------------------------------------------------------\n";

    for (const auto& disk : disks) {
        std::cout << std::left
                  << std::setw(15) << disk.path
                  << std::setw(15) << FormatBytes(disk.totalBytes)
                  << std::setw(15) << FormatBytes(disk.freeBytes)
                  << std::setw(15) << FormatBytes(disk.availableBytes)
                  << "\n";
    }
}

std::string DiskMonitor::FormatBytes(unsigned long long bytes)
{
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};

    double size = static_cast<double>(bytes);
    int unit = 0;

    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        unit++;
    }

    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << size << " " << units[unit];

    return out.str();
}