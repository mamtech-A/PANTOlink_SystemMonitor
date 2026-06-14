#ifndef JSON_MAKER_HPP
#define JSON_MAKER_HPP

#include "bootTimeMonitor.hpp"
#include "cpuMonitor.hpp"
#include "diskMonitor.hpp"
#include "internetConnectionMonitor.hpp"
#include "RAMMonitor.hpp"

#include <string>

class JSONMaker {
public:
    JSONMaker(CPUMonitor& cpuMonitor,
              RAMMonitor& ramMonitor,
              DiskMonitor& diskMonitor,
              InternetConnectionMonitor& internetMonitor,
              BootTimeMonitor& bootTimeMonitor);

    // Full JSON
    std::string makeFullJson() const;
    void printFullJson() const;

    // Go-style transmitted message outputs
    std::string makeGoPingJson() const;
    std::string makeGoLiveJson() const;
    std::string makeGoFullJson() const;

    void printGoPingJson() const;
    void printGoLiveJson() const;
    void printGoFullJson() const;

private:
    std::string escapeJson(const std::string& text) const;
    std::string formatDouble(double value, int precision = 2) const;

    long long getCurrentTimestampMs() const;
    long long getLiveGoTimestampMs() const;
    long long getFullGoTimestampMs() const;

private:
    CPUMonitor& cpuMonitor_;
    RAMMonitor& ramMonitor_;
    DiskMonitor& diskMonitor_;
    InternetConnectionMonitor& internetMonitor_;
    BootTimeMonitor& bootTimeMonitor_;
};

#endif