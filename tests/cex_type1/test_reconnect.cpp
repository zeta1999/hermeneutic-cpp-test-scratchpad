#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPServerParams.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/Net/WebSocket.h>
#include <Poco/Timespan.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "hermeneutic/cex_type1/feed.hpp"

namespace {
class BurstHandler : public Poco::Net::HTTPRequestHandler {
 public:
  BurstHandler(std::vector<std::string> payloads, std::atomic<int>& counter)
      : payloads_(std::move(payloads)), counter_(counter) {}

  void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override {
    try {
      Poco::Net::WebSocket ws(request, response);
      ws.setSendTimeout(Poco::Timespan(1, 0));
      ws.setReceiveTimeout(Poco::Timespan(1, 0));
      counter_.fetch_add(1);
      for (const auto& payload : payloads_) {
        ws.sendFrame(payload.data(), static_cast<int>(payload.size()), Poco::Net::WebSocket::FRAME_TEXT);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
    } catch (...) {
    }
  }

 private:
  std::vector<std::string> payloads_;
  std::atomic<int>& counter_;
};

class BurstFactory : public Poco::Net::HTTPRequestHandlerFactory {
 public:
  BurstFactory(std::vector<std::string> payloads, std::atomic<int>& counter)
      : payloads_(std::move(payloads)), counter_(counter) {}

  Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest&) override {
    return new BurstHandler(payloads_, counter_);
  }

 private:
  std::vector<std::string> payloads_;
  std::atomic<int>& counter_;
};

}  // namespace

TEST_CASE("cex_type1 feed reconnects after server restart") {
  using namespace std::chrono_literals;
  std::signal(SIGPIPE, SIG_IGN);

  const std::vector<std::string> payloads = {
      "{\"type\":\"new_order\",\"sequence\":1,\"order_id\":201,\"side\":\"bid\",\"price\":\"90.00\",\"quantity\":\"1\"}",
      "{\"type\":\"new_order\",\"sequence\":2,\"order_id\":202,\"side\":\"bid\",\"price\":\"91.00\",\"quantity\":\"1\"}"
  };

  Poco::UInt16 port = 0;
  try {
    Poco::Net::ServerSocket probe;
    probe.bind(Poco::Net::SocketAddress("127.0.0.1", 0));
    probe.listen();
    port = probe.address().port();
  } catch (const Poco::Exception& ex) {
    std::cerr << "Skipping reconnect test: " << ex.displayText() << std::endl;
    return;
  }
  auto makeSocket = [port]() {
    Poco::Net::ServerSocket sock;
    sock.bind(Poco::Net::SocketAddress("127.0.0.1", port), true);
    sock.listen();
    return sock;
  };
  Poco::Net::HTTPServerParams::Ptr params = new Poco::Net::HTTPServerParams;
  params->setMaxThreads(1);
  params->setMaxQueued(1);
  std::atomic<int> connection_count{0};

  auto runServer = [&](std::atomic<bool>& stop_flag) {
    auto local_socket = makeSocket();
    Poco::Net::HTTPServer server(new BurstFactory(payloads, connection_count), local_socket, params);
    server.start();
    while (!stop_flag.load()) {
      std::this_thread::sleep_for(20ms);
    }
    server.stop();
  };

  std::atomic<bool> stop_flag{false};
  std::thread server_thread(runServer, std::ref(stop_flag));

  std::mutex mutex;
  std::condition_variable cv;
  std::vector<std::uint64_t> received_sequences;

  auto feed = hermeneutic::cex_type1::makeWebSocketFeed(
      {.exchange = "reconnect",
       .url = "ws://127.0.0.1:" + std::to_string(port),
       .auth_token = "",
       .interval = 20ms},
      [&](hermeneutic::common::BookEvent evt) {
        std::lock_guard<std::mutex> lock(mutex);
        received_sequences.push_back(evt.sequence);
        cv.notify_one();
      });

  feed->start();
  {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait_for(lock, 1s, [&] { return received_sequences.size() >= 2; });
  }

  // Simulate server crash
  stop_flag = true;
  server_thread.join();
  received_sequences.clear();

  // Restart server
  stop_flag = false;
  std::thread server_thread2(runServer, std::ref(stop_flag));

  {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait_for(lock, 1s, [&] { return received_sequences.size() >= 2; });
  }

  stop_flag = true;
  server_thread2.join();
  feed->stop();

  CHECK(connection_count.load() >= 2);
  CHECK(received_sequences.size() >= 2);
}
