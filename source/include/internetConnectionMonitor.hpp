#ifndef INTERNET_CONNECTION_MONITOR_HPP
#define INTERNET_CONNECTION_MONITOR_HPP

#include <string>

class InternetConnectionMonitor {
public:
    InternetConnectionMonitor();

    bool isConnected() const;
    void update();
    void sample();

    unsigned long long getLastRxBytes() const;
    unsigned long long getLastTxBytes() const;
    unsigned long long getLastRxDiff() const;
    unsigned long long getLastTxDiff() const;
    bool getLastConnectionState() const;

private:
    bool pingHost(const std::string& host) const;
    void readNetworkBytes(unsigned long long& rx, unsigned long long& tx) const;

private:
    unsigned long long prevRxBytes;
    unsigned long long prevTxBytes;

    unsigned long long lastRxBytes_;
    unsigned long long lastTxBytes_;
    unsigned long long lastRxDiff_;
    unsigned long long lastTxDiff_;
    bool lastConnected_;
};

#endif