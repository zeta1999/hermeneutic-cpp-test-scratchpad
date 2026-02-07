#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/Net/StreamSocket.h>
#include <Poco/Process.h>

#include <chrono>
#include <condition_variable>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#include "hermeneutic/cex_type1/feed.hpp"

namespace {
std::filesystem::path findCexServiceBinary() {
  std::filesystem::path binary(PROJECT_BINARY_DIR);
  binary /= "services/cex_type1_service/cex_type1_service";
  return binary;
}

bool canBindLoopback() {
  try {
    Poco::Net::ServerSocket probe(Poco::Net::SocketAddress("127.0.0.1", 0));
    (void)probe.address();
    return true;
  } catch (const Poco::Exception& ex) {
    std::cerr << "Skipping cex_type1_service test: " << ex.displayText() << std::endl;
    return false;
  }
}

}  // namespace

TEST_CASE("cex_type1_service/executable streams payloads") {
  using namespace std::chrono_literals;
  if (!canBindLoopback()) {
    return;
  }

  auto service_binary = findCexServiceBinary();
  if (!std::filesystem::exists(service_binary)) {
    std::cerr << "Skipping cex_type1_service test: " << service_binary << " missing" << std::endl;
    return;
  }

  std::signal(SIGPIPE, SIG_IGN);

  auto temp_file = std::filesystem::temp_directory_path() / "cex_type1_service_test.ndjson";
  {
    std::ofstream out(temp_file);
    out << "{\"type\":\"new_order\",\"sequence\":1,\"order_id\":101,\"side\":\"bid\",\"price\":\"90.25\",\"quantity\":\"1.5\"}" << std::endl;
    out << "{\"type\":\"new_order\",\"sequence\":2,\"order_id\":102,\"side\":\"bid\",\"price\":\"101.50\",\"quantity\":\"0.4\"}" << std::endl;
  }

  Poco::UInt16 port = 0;
  try {
    Poco::Net::ServerSocket probe(Poco::Net::SocketAddress("127.0.0.1", 0));
    port = probe.address().port();
  } catch (const Poco::Exception& ex) {
    std::cerr << "Skipping cex_type1_service test: " << ex.displayText() << std::endl;
    std::filesystem::remove(temp_file);
    return;
  }

  const std::string exchange = "svc-test";
const std::string token = "svc-token";
std::vector<std::string> args = {
      exchange,
      temp_file.string(),
      std::to_string(port),
      token,
      "10",
      "2",
  };

  std::unique_ptr<Poco::ProcessHandle> handle;
  try {
    handle = std::make_unique<Poco::ProcessHandle>(
        Poco::Process::launch(service_binary.string(), args));
  } catch (const Poco::Exception& ex) {
    std::cerr << "Skipping cex_type1_service test: " << ex.displayText() << std::endl;
    std::filesystem::remove(temp_file);
    return;
  }

  auto cleanup = [&]() {
    if (!handle) {
      return;
    }
    try {
      Poco::Process::kill(*handle);
    } catch (...) {
    }
    try {
      handle->wait();
    } catch (...) {
    }
    std::filesystem::remove(temp_file);
  };

  std::mutex mutex;
  std::condition_variable cv;
  std::vector<hermeneutic::common::BookEvent> received_events;

  auto waitForServer = [&](int attempts) {
    for (int i = 0; i < attempts; ++i) {
      try {
        Poco::Net::StreamSocket probe;
        probe.connect(Poco::Net::SocketAddress("127.0.0.1", port));
        probe.close();
        return true;
      } catch (...) {
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
      }
    }
    return false;
  };

  if (!waitForServer(50)) {
    cleanup();
    std::cerr << "cex_type1_service did not accept connections in time" << std::endl;
    CHECK(false);
    return;
  }

  auto feed = hermeneutic::cex_type1::makeWebSocketFeed(
      {.exchange = exchange,
       .url = "ws://127.0.0.1:" + std::to_string(port) + "/" + exchange,
       .auth_token = token,
       .interval = 20ms},
      [&](hermeneutic::common::BookEvent update) {
        std::lock_guard<std::mutex> lock(mutex);
        received_events.push_back(std::move(update));
        cv.notify_one();
      });

  auto has_target = [&]() {
    return std::any_of(received_events.begin(), received_events.end(), [](const auto& evt) {
      return evt.sequence >= 2;
    });
  };

  feed->start();
  {
    std::unique_lock<std::mutex> lock(mutex);
    if (!cv.wait_for(lock, 3s, [&] { return has_target(); })) {
      feed->stop();
      cleanup();
      std::cerr << "cex_type1_service test did not receive sequence >= 2" << std::endl;
      CHECK(false);
      return;
    }
  }
  feed->stop();
  cleanup();

  auto target = std::find_if(received_events.begin(), received_events.end(), [](const auto& evt) {
    return evt.sequence >= 2;
  });
  if (target == received_events.end()) {
    CHECK(false);
    return;
  }
  CHECK(target->exchange == exchange);
  CHECK(target->kind == hermeneutic::common::BookEventKind::NewOrder);
  CHECK(target->sequence == 2);
  CHECK(target->order.order_id == 102);
  CHECK(target->order.side == hermeneutic::common::Side::Bid);
  CHECK(target->order.price == hermeneutic::common::Decimal::fromString("101.50"));
  CHECK(target->order.quantity == hermeneutic::common::Decimal::fromString("0.4"));
}
