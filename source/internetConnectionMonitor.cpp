#include "internetConnectionMonitor.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

InternetConnectionMonitor::InternetConnectionMonitor()
    : prevRxBytes(0),
      prevTxBytes(0),
      lastRxBytes_(0),
      lastTxBytes_(0),
      lastRxDiff_(0),
      lastTxDiff_(0),
      lastConnected_(false) {
}

bool InternetConnectionMonitor::pingHost(const std::string& host) const {
    std::string command = "ping -c 1 -W 2 " + host + " > /dev/null 2>&1";
    int result = std::system(command.c_str());
    return result == 0;
}

bool InternetConnectionMonitor::isConnected() const {
    return pingHost("8.8.8.8") || pingHost("1.1.1.1");
}

void InternetConnectionMonitor::readNetworkBytes(unsigned long long& rx,
                                                 unsigned long long& tx) const {
    std::ifstream file("/proc/net/dev");
    std::string line;

    rx = 0;
    tx = 0;

    while (std::getline(file, line)) {
        if (line.find(':') == std::string::npos) {
            continue;
        }

        std::istringstream iss(line);
        std::string iface;
        unsigned long long r_bytes = 0;
        unsigned long long t_bytes = 0;

        iss >> iface;
        iface = iface.substr(0, iface.find(':'));

        if (iface == "lo") {
            continue;
        }

        iss >> r_bytes;

        unsigned long long temp = 0;
        for (int i = 0; i < 7; ++i) {
            iss >> temp;
        }

        iss >> t_bytes;

        rx += r_bytes;
        tx += t_bytes;
    }
}

void InternetConnectionMonitor::sample() {
    unsigned long long currentRx = 0;
    unsigned long long currentTx = 0;
    readNetworkBytes(currentRx, currentTx);

    if (prevRxBytes == 0 && prevTxBytes == 0) {
        prevRxBytes = currentRx;
        prevTxBytes = currentTx;
    }

    lastRxDiff_ = currentRx - prevRxBytes;
    lastTxDiff_ = currentTx - prevTxBytes;

    prevRxBytes = currentRx;
    prevTxBytes = currentTx;

    lastRxBytes_ = currentRx;
    lastTxBytes_ = currentTx;
    lastConnected_ = isConnected();
}

void InternetConnectionMonitor::update() {
    sample();

    if (lastConnected_) {
        std::cout << "Internet connection is available.\n";
    } else {
        std::cout << "No internet connection.\n";
    }

    std::cout << "Download (RX): " << lastRxDiff_ << " bytes\n";
    std::cout << "Upload (TX):   " << lastTxDiff_ << " bytes\n";
}

unsigned long long InternetConnectionMonitor::getLastRxBytes() const {
    return lastRxBytes_;
}

unsigned long long InternetConnectionMonitor::getLastTxBytes() const {
    return lastTxBytes_;
}

unsigned long long InternetConnectionMonitor::getLastRxDiff() const {
    return lastRxDiff_;
}

unsigned long long InternetConnectionMonitor::getLastTxDiff() const {
    return lastTxDiff_;
}

bool InternetConnectionMonitor::getLastConnectionState() const {
    return lastConnected_;
}