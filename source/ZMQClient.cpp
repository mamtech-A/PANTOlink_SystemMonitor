#include "ZMQClient.hpp"

#include <ctime>
#include <iostream>
#include <thread>

ZMQClient::ZMQClient(CPUMonitor &cpuMonitor, RAMMonitor &ramMonitor,
                     DiskMonitor &diskMonitor,
                     InternetConnectionMonitor &internetMonitor,
                     BootTimeMonitor &bootTimeMonitor,
                     const std::string &serverAddress)
    : cpuMonitor_(cpuMonitor), ramMonitor_(ramMonitor),
      diskMonitor_(diskMonitor), internetMonitor_(internetMonitor),
      bootTimeMonitor_(bootTimeMonitor),
      jsonMaker_(cpuMonitor_, ramMonitor_, diskMonitor_, internetMonitor_,
                 bootTimeMonitor_),
      context_(1), socket_(nullptr), serverAddress_(serverAddress),
      connected_(false), lastFullSent_(), fullSentCount_(0), liveSentCount_(0),
      skipCount_(0), errorCount_(0) {}

ZMQClient::~ZMQClient() { close(); }

bool ZMQClient::connect() {
  close();

  try {
    socket_ =
        std::make_unique<zmq::socket_t>(context_, zmq::socket_type::dealer);

    socket_->set(zmq::sockopt::sndtimeo, kMaxSendTimeoutMs);
    socket_->set(zmq::sockopt::rcvtimeo, kMaxSendTimeoutMs);
    socket_->set(zmq::sockopt::linger, 0);

    std::cout << "[CONNECT] Attempting to connect to " << serverAddress_
              << '\n';
    socket_->connect(serverAddress_);

    if (!sendPingTest()) {
      connected_ = false;
      return false;
    }

    connected_ = true;
    std::cout << "[CONNECT] Connection test successful\n";
    return true;
  } catch (const std::exception &ex) {
    std::cout << "[CONNECT] Failed: " << ex.what() << '\n';
    connected_ = false;
    return false;
  }
}

bool ZMQClient::sendPingTest() {
  if (!socket_) {
    return false;
  }

  const std::string pingJson = jsonMaker_.makeGoPingJson();

  // std::cout << "\n========== ZMQ PING JSON ==========\n";
  // std::cout << pingJson << '\n';
  // std::cout << "===================================\n";

  zmq::message_t request(pingJson.begin(), pingJson.end());

  try {
    if (!socket_->send(request, zmq::send_flags::none)) {
      std::cout << "[CONNECT] Failed to send test message\n";
      return false;
    }

    zmq::message_t reply;
    if (!socket_->recv(reply, zmq::recv_flags::none)) {
      std::cout << "[CONNECT] Connection test timed out\n";
      return false;
    }

    std::cout << "[CONNECT] Received: "
              << std::string(static_cast<char *>(reply.data()), reply.size())
              << '\n';
    return true;
  } catch (const std::exception &ex) {
    std::cout << "[CONNECT] Connection test failed: " << ex.what() << '\n';
    return false;
  }
}

bool ZMQClient::sendMessage(const std::string &data) {
  if (!connected_ || !socket_) {
    std::cout << "[SEND] Not connected to server\n";
    return false;
  }

  try {
    std::cout << "[SEND] Sending " << data.size() << " bytes to server\n";
    // std::cout << "[SEND][JSON] " << data << '\n';

    zmq::message_t request(data.begin(), data.end());
    if (!socket_->send(request, zmq::send_flags::none)) {
      std::cout << "[SEND] Send failed\n";
      connected_ = false;
      return false;
    }

    zmq::message_t reply;
    if (!socket_->recv(reply, zmq::recv_flags::none)) {
      std::cout << "[RECV] Receive timeout\n";
      connected_ = false;
      return false;
    }

    std::cout << "[RECV] Received response: "
              << std::string(static_cast<char *>(reply.data()), reply.size())
              << '\n';
    return true;
  } catch (const std::exception &ex) {
    std::cout << "[SEND/RECV] Error: " << ex.what() << '\n';
    connected_ = false;
    return false;
  }
}

void ZMQClient::close() {
  if (socket_) {
    try {
      socket_->close();
    } catch (...) {
    }
    socket_.reset();
  }

  connected_ = false;
}

bool ZMQClient::isTimeForFullData(
    const std::chrono::system_clock::time_point &currentTime) const {
  if (lastFullSent_.time_since_epoch().count() == 0) {
    return true;
  }

  const auto currentMinute =
      std::chrono::time_point_cast<std::chrono::minutes>(currentTime);
  const auto lastFullMinute =
      std::chrono::time_point_cast<std::chrono::minutes>(lastFullSent_);

  return currentMinute != lastFullMinute;
}

void ZMQClient::sampleAllMonitors() {
  cpuMonitor_.sample();
  ramMonitor_.sample();
  diskMonitor_.sample();
  internetMonitor_.sample();
}

bool ZMQClient::sendFullData() {
  const std::string fullJson = jsonMaker_.makeGoFullJson();

  // std::cout << "\n========== ZMQ FULL JSON ==========\n";
  // std::cout << fullJson << '\n';
  // std::cout << "===================================\n";

  if (!sendMessage(fullJson)) {
    ++errorCount_;
    return false;
  }

  lastFullSent_ = std::chrono::system_clock::now();
  ++fullSentCount_;

  std::cout << "[CYCLE] FULL metrics sent successfully (total FULL sent: "
            << fullSentCount_ << ")\n";
  return true;
}

bool ZMQClient::sendLiveData() {
  const std::string liveJson = jsonMaker_.makeGoLiveJson();

  // std::cout << "\n========== ZMQ LIVE JSON ==========\n";
  // std::cout << liveJson << '\n';
  // std::cout << "===================================\n";

  if (!sendMessage(liveJson)) {
    ++errorCount_;
    return false;
  }

  ++liveSentCount_;
  std::cout << "[CYCLE] LIVE metrics sent successfully (total LIVE sent: "
            << liveSentCount_ << ")\n";
  return true;
}

void ZMQClient::resetCountersIfNeeded() {
  if (fullSentCount_ >= kCounterResetAt || liveSentCount_ >= kCounterResetAt) {
    std::cout << "[COUNTER] Resetting counters to prevent overflow\n";
    fullSentCount_ = 0;
    liveSentCount_ = 0;
    skipCount_ = 0;
    errorCount_ = 0;
  }
}

void ZMQClient::runMetricsLoop() {
  int reconnectAttempts = 0;

  while (true) {
    if (!connected_) {
      const std::string fullJson = jsonMaker_.makeGoFullJson();

      // std::cout << "\n========== ZMQ FULL JSON ==========\n";
      // std::cout << fullJson << '\n';
      // std::cout << "===================================\n";
      if (reconnectAttempts >= kMaxReconnectAttempts) {
        std::cout << "[ERROR] Max reconnect attempts (" << kMaxReconnectAttempts
                  << ") reached, stopping\n";
        return;
      }

      ++reconnectAttempts;
      std::cout << "[RECONNECT] Attempting to reconnect (" << reconnectAttempts
                << "/" << kMaxReconnectAttempts << ")\n";

      if (!connect()) {
        ++skipCount_;
        std::cout << "[RECONNECT] Connection failed\n";
        std::this_thread::sleep_for(
            std::chrono::milliseconds(kReconnectDelayMs));
        continue;
      }

      reconnectAttempts = 0;
      std::cout << "[RECONNECT] Successfully reconnected\n";
    }

    resetCountersIfNeeded();

    const auto currentTime = std::chrono::system_clock::now();
    const bool needsFullData = isTimeForFullData(currentTime);

    const std::time_t now = std::chrono::system_clock::to_time_t(currentTime);
    std::cout << "\n[CYCLE] Starting metrics collection ("
              << (needsFullData ? "FULL + LIVE" : "LIVE") << ") at "
              << std::ctime(&now);

    std::cout << "[STATS] Sent: FULL=" << fullSentCount_
              << ", LIVE=" << liveSentCount_ << ", Skipped=" << skipCount_
              << ", Errors=" << errorCount_ << '\n';

    sampleAllMonitors();

    std::cout << "[METRICS] CPU: " << cpuMonitor_.getTotalCPUUsage()
              << "%, Memory: " << ramMonitor_.getMemoryPercentUsed()
              << "%, Disk: " << diskMonitor_.getDiskPercentUsed() << "%\n";

    if (needsFullData) {
      if (!sendFullData()) {
        std::cout << "[CYCLE] Failed to send FULL metrics\n";
        std::this_thread::sleep_for(
            std::chrono::milliseconds(kMetricIntervalMs));
        continue;
      }
    }

    if (!sendLiveData()) {
      std::cout << "[CYCLE] Failed to send LIVE metrics\n";
      std::this_thread::sleep_for(std::chrono::milliseconds(kMetricIntervalMs));
      continue;
    }

    // std::cout << "[SUMMARY] CPU: " << cpuMonitor_.getTotalCPUUsage()
    //           << "%, Memory: " << ramMonitor_.getMemoryPercentUsed()
    //           << "%, Disk: " << diskMonitor_.getDiskPercentUsed() << "%\n";
    // std::cout << "[CYCLE] End of metrics collection\n";

    std::this_thread::sleep_for(std::chrono::milliseconds(kMetricIntervalMs));
  }
}