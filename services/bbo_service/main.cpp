#include <spdlog/spdlog.h>

#include <atomic>
#include <csignal>
#include <chrono>
#include <filesystem>
#include <fstream>
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
  std::string csv_path = "bbo_quotes.csv";
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

  bool need_header = true;
  if (std::filesystem::exists(csv_path) && std::filesystem::file_size(csv_path) > 0) {
    need_header = false;
  }
  std::ofstream csv(csv_path, std::ios::app);
  if (!csv.is_open()) {
    spdlog::error("Failed to open {} for writing", csv_path);
    return 1;
  }
  if (need_header) {
    csv << "timestamp_ns,symbol,best_bid_price,best_bid_quantity,best_ask_price,best_ask_quantity,exchange_count,"
        << "last_feed_timestamp_ns,last_local_timestamp_ns,min_feed_timestamp_ns,max_feed_timestamp_ns,"
        << "min_local_timestamp_ns,max_local_timestamp_ns,publish_timestamp_ns\n";
    csv.flush();
  }

  hermeneutic::bbo::BboPublisher publisher;
  hermeneutic::services::BookStreamClient client(
      endpoint,
      token,
      symbol,
      [&](const hermeneutic::common::AggregatedBookView& view) {
        auto timestamp_ns = view.publish_timestamp_ns;
        if (timestamp_ns == 0) {
          timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(view.timestamp.time_since_epoch()).count();
        }
        csv << timestamp_ns << ','
            << symbol << ','
            << view.best_bid.price.toString(8) << ','
            << view.best_bid.quantity.toString(8) << ','
            << view.best_ask.price.toString(8) << ','
            << view.best_ask.quantity.toString(8) << ','
            << view.exchange_count << ','
            << view.last_feed_timestamp_ns << ','
            << view.last_local_timestamp_ns << ','
            << view.min_feed_timestamp_ns << ','
            << view.max_feed_timestamp_ns << ','
            << view.min_local_timestamp_ns << ','
            << view.max_local_timestamp_ns << ','
            << timestamp_ns << '\n';
        csv.flush();
        spdlog::info(publisher.format(view));
      });
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
