#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/SocketAddress.h>
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
    out << "{\"side\":\"bid\",\"price\":\"100.25\",\"quantity\":\"1.5\"}" << std::endl;
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
  bool received = false;
  hermeneutic::common::MarketUpdate captured;

  auto feed = hermeneutic::cex_type1::makeWebSocketFeed(
      {.exchange = exchange,
       .url = "ws://127.0.0.1:" + std::to_string(port) + "/" + exchange,
       .auth_token = token,
       .interval = 20ms},
      [&](hermeneutic::common::MarketUpdate update) {
        std::lock_guard<std::mutex> lock(mutex);
        captured = std::move(update);
        received = true;
        cv.notify_one();
      });

  feed->start();
  {
    std::unique_lock<std::mutex> lock(mutex);
    if (!cv.wait_for(lock, 3s, [&] { return received; })) {
      feed->stop();
      cleanup();
      std::cerr << "cex_type1_service test did not receive update" << std::endl;
      CHECK(false);
      return;
    }
  }
  feed->stop();
  cleanup();

  CHECK(captured.exchange == exchange);
  CHECK(captured.side == hermeneutic::common::Side::Bid);
  CHECK(captured.price == hermeneutic::common::Decimal::fromString("100.25"));
  CHECK(captured.quantity == hermeneutic::common::Decimal::fromString("1.5"));
}
