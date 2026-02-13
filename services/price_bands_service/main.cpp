#include <spdlog/spdlog.h>

#include <atomic>
#include <csignal>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include "hermeneutic/price_bands/price_bands_publisher.hpp"
#include "hermeneutic/common/assert.hpp"
#include "common/book_stream_client.hpp"
#include "common/csv_utils.hpp"

namespace {
std::atomic<bool> g_running{true};

void handleSignal(int) {
  g_running = false;
}
}  // namespace

int main(int argc, char** argv) {
  spdlog::set_level(spdlog::level::debug);
  std::string endpoint = "127.0.0.1:50051";
  std::string token = "";
  std::string symbol = "BTCUSDT";
  std::string csv_path = "price_bands.csv";
  if (argc > 1) {
    endpoint = argv[1];
  }
  if (argc > 2) {
    token = argv[2];
  }
  if (argc > 3) {
    symbol = argv[3];
  }
  if (argc > 4) {
    csv_path = argv[4];
  }

  constexpr std::string_view kHeader =
      "timestamp_ns,symbol,offset_bps,bid_price,ask_price,last_feed_timestamp_ns,last_local_timestamp_ns,"
      "min_feed_timestamp_ns,max_feed_timestamp_ns,min_local_timestamp_ns,max_local_timestamp_ns,publish_timestamp_ns";
  if (!hermeneutic::services::ensureCsvHasHeader(csv_path, kHeader)) {
    return 1;
  }
  std::ofstream csv(csv_path, std::ios::app);
  if (!csv.is_open()) {
    spdlog::error("Failed to open {} for writing", csv_path);
    return 1;
  }

  auto calculator = hermeneutic::price_bands::PriceBandsCalculator(
      hermeneutic::price_bands::defaultOffsets());
  hermeneutic::services::BookStreamClient client(
      endpoint,
      token,
      symbol,
      [&](const hermeneutic::common::AggregatedBookView& view) {
        auto timestamp_ns = view.publish_timestamp_ns;
        if (timestamp_ns == 0) {
          timestamp_ns =
              std::chrono::duration_cast<std::chrono::nanoseconds>(view.timestamp.time_since_epoch()).count();
        }
        spdlog::debug("price_bands_service snapshot: publish_ts={} best_bid={} qty={} best_ask={} qty={} exchanges={}",
                      timestamp_ns,
                      view.best_bid.price.toString(8), view.best_bid.quantity.toString(8),
                      view.best_ask.price.toString(8), view.best_ask.quantity.toString(8),
                      view.exchange_count);
        if (view.best_bid.quantity > hermeneutic::common::Decimal::fromRaw(0) &&
            view.best_ask.quantity > hermeneutic::common::Decimal::fromRaw(0)) {
          HERMENEUTIC_ASSERT_DEBUG(view.best_ask.price >= view.best_bid.price,
                                   "price bands export detected crossed book");
        }
        auto quotes = calculator.compute(view);
        if (quotes.empty()) {
          spdlog::warn("price_bands_service: calculator returned 0 bands for snapshot timestamp {}", timestamp_ns);
        }
        spdlog::debug("price_bands_service: writing {} bands", quotes.size());
        for (const auto& quote : quotes) {
          csv << timestamp_ns << ','
              << symbol << ','
              << quote.offset_bps.toString(0) << ','
              << quote.bid_price.toString(8) << ','
              << quote.ask_price.toString(8) << ','
              << view.last_feed_timestamp_ns << ','
              << view.last_local_timestamp_ns << ','
              << view.min_feed_timestamp_ns << ','
              << view.max_feed_timestamp_ns << ','
              << view.min_local_timestamp_ns << ','
              << view.max_local_timestamp_ns << ','
              << timestamp_ns << '\n';
          spdlog::info(hermeneutic::price_bands::formatQuote(quote));
        }
        csv.flush();
      });

  client.start();
  spdlog::info("Price bands client streaming from {} token='{}' symbol='{}' output={}"
               , endpoint, token, symbol, csv_path);
  std::signal(SIGINT, handleSignal);
  std::signal(SIGTERM, handleSignal);
  while (g_running.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  client.stop();
  return 0;
}
