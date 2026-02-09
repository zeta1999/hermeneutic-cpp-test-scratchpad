#include <spdlog/spdlog.h>

#include <atomic>
#include <csignal>
#include <chrono>
#include <string>
#include <thread>

#include "hermeneutic/price_bands/price_bands_publisher.hpp"
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

  auto calculator = hermeneutic::price_bands::PriceBandsCalculator(
      hermeneutic::price_bands::defaultOffsets());
  hermeneutic::services::BookStreamClient client(
      endpoint,
      token,
      symbol,
      [&](const hermeneutic::common::AggregatedBookView& view) {
        auto quotes = calculator.compute(view);
        for (const auto& quote : quotes) {
          spdlog::info("Offset {} bps -> bid {} ask {}",
                       quote.offset_bps.toString(0),
                       quote.bid_price.toString(2),
                       quote.ask_price.toString(2));
        }
      });

  client.start();
  spdlog::info("Price bands client streaming from {}", endpoint);
  std::signal(SIGINT, handleSignal);
  std::signal(SIGTERM, handleSignal);
  while (g_running.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  client.stop();
  return 0;
}
