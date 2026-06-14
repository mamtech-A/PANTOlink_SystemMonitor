#include "bootTimeMonitor.hpp"
#include "cpuMonitor.hpp"
#include "diskMonitor.hpp"
#include "internetConnectionMonitor.hpp"
#include "RAMMonitor.hpp"
#include "ZMQClient.hpp"

#include <exception>
#include <iostream>
#include <string>

int main() {
    try {
        CPUMonitor cpuMonitor;
        RAMMonitor ramMonitor;
        DiskMonitor diskMonitor;
        InternetConnectionMonitor internetMonitor;
        BootTimeMonitor bootTimeMonitor;

        const std::string serverAddress = "tcp://localhost:5555";

        ZMQClient zmqClient(cpuMonitor,
                            ramMonitor,
                            diskMonitor,
                            internetMonitor,
                            bootTimeMonitor,
                            serverAddress);

        zmqClient.runMetricsLoop();
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "Fatal error: unknown exception\n";
        return 1;
    }

    return 0;
}