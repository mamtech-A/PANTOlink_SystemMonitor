#ifndef BOOTTIMEMONITOR_HPP
#define BOOTTIMEMONITOR_HPP

#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

class BootTimeMonitor {
public:
    std::time_t getLastBootTime() const {
        double uptimeSeconds = readUptimeSeconds();
        std::time_t now = std::time(nullptr);
        return now - static_cast<std::time_t>(uptimeSeconds);
    }

    std::string getLastBootTimeString() const {
        std::time_t bootTime = getLastBootTime();

        std::tm* utcTime = std::gmtime(&bootTime);
        if (utcTime == nullptr) {
            throw std::runtime_error("Failed to convert boot time to UTC");
        }

        std::ostringstream output;
        output << std::put_time(utcTime, "%Y-%m-%dT%H:%M:%SZ");
        return output.str();
    }

    std::time_t getCurrentTime() const {
        return std::time(nullptr);
    }

    std::string getCurrentTimeString() const {
        std::time_t currentTime = getCurrentTime();

        std::tm* localTime = std::localtime(&currentTime);
        if (localTime == nullptr) {
            throw std::runtime_error("Failed to convert current time to local time");
        }

        std::ostringstream output;
        output << std::put_time(localTime, "%Y-%m-%dT%H:%M:%S");
        return output.str();
    }

    std::string getCurrentMinuteString() const {
        std::time_t currentTime = getCurrentTime();
        currentTime = currentTime - (currentTime % 60);

        std::tm* localTime = std::localtime(&currentTime);
        if (localTime == nullptr) {
            throw std::runtime_error("Failed to convert current minute to local time");
        }

        std::ostringstream output;
        output << std::put_time(localTime, "%Y-%m-%dT%H:%M:%S");
        return output.str();
    }

    std::string getCurrentTimeStringUTC() const {
        std::time_t currentTime = getCurrentTime();

        std::tm* utcTime = std::gmtime(&currentTime);
        if (utcTime == nullptr) {
            throw std::runtime_error("Failed to convert current time to UTC");
        }

        std::ostringstream output;
        output << std::put_time(utcTime, "%Y-%m-%dT%H:%M:%SZ");
        return output.str();
    }

    double readUptimeSeconds() const {
        std::ifstream uptimeFile("/proc/uptime");
        if (!uptimeFile.is_open()) {
            throw std::runtime_error("Cannot open /proc/uptime");
        }

        double uptimeSeconds = 0.0;
        uptimeFile >> uptimeSeconds;

        if (uptimeFile.fail()) {
            throw std::runtime_error("Failed to read uptime");
        }

        return uptimeSeconds;
    }
};

#endif