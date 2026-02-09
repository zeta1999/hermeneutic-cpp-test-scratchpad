#include <spdlog/spdlog.h>

#include <atomic>
#include <csignal>
#include <chrono>
#include <string>
#include <thread>

#include "hermeneutic/bbo/bbo_publisher.hpp"
#include "common/book_stream_client.hpp"

namespace {
std::atomic<bool> g_running{true};

void handleSignal(int) {
  g_running = false;
}
}  // namespace

int main(int argc, char** argv) {
  std::string endpoint = "127.0.0.1:50051";
  std::string token = "";
  std::string symbol = "BTCUSDT";
  if (argc > 1) {
    endpoint = argv[1];
  }
  if (argc > 2) {
    token = argv[2];
  }
  if (argc > 3) {
    symbol = argv[3];
  }

  hermeneutic::bbo::BboPublisher publisher;
  hermeneutic::services::BookStreamClient client(
      endpoint,
      token,
      symbol,
      [&](const hermeneutic::common::AggregatedBookView& view) { spdlog::info(publisher.format(view)); });
  client.start();
  spdlog::info("BBO client streaming from {}", endpoint);

  std::signal(SIGINT, handleSignal);
  std::signal(SIGTERM, handleSignal);

  while (g_running.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  client.stop();
  return 0;
}
