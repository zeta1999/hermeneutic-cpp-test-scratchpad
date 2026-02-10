#include <chrono>
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

#include "hermeneutic/aggregator/aggregator.hpp"
#include "hermeneutic/common/events.hpp"
#include "tests/support/test_data_factory.hpp"

using hermeneutic::common::Decimal;
using hermeneutic::common::Side;

using hermeneutic::tests::support::makeNewOrder;
using hermeneutic::tests::support::timeFromNanoseconds;

TEST_CASE("aggregator consolidates best bid and ask") {
  hermeneutic::aggregator::AggregationEngine engine;
  engine.start();
  engine.push(makeNewOrder("notbinance", 1, Side::Bid, "100.00", "1", 1, timeFromNanoseconds(100)));
  engine.push(makeNewOrder("notcoinbase", 2, Side::Bid, "101.00", "2", 2, timeFromNanoseconds(200)));
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  auto view = engine.latest();
  CHECK(view.best_bid.price.toString(2) == "101.00");
  CHECK(view.best_bid.quantity.toString(0) == "2");
  CHECK(view.last_feed_timestamp_ns >= 200);
  CHECK(view.max_feed_timestamp_ns >= view.last_feed_timestamp_ns);
  CHECK(view.min_feed_timestamp_ns > 0);
  CHECK(view.last_local_timestamp_ns >= view.min_local_timestamp_ns);
  engine.stop();
}

TEST_CASE("aggregator exposes aggregated depth levels") {
  hermeneutic::aggregator::AggregationEngine engine;
  engine.start();
  engine.push(makeNewOrder("ex1", 1, Side::Bid, "100.00", "1", 1, timeFromNanoseconds(1000)));
  engine.push(makeNewOrder("ex1", 2, Side::Bid, "101.00", "2", 2, timeFromNanoseconds(2000)));
  engine.push(makeNewOrder("ex2", 3, Side::Bid, "100.00", "3", 3, timeFromNanoseconds(3000)));
  engine.push(makeNewOrder("ex2", 4, Side::Ask, "105.00", "4", 4, timeFromNanoseconds(4000)));
  engine.push(makeNewOrder("ex3", 5, Side::Ask, "106.00", "5", 5, timeFromNanoseconds(5000)));
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  auto view = engine.latest();
  CHECK(view.bid_levels.size() >= 2);
  CHECK(view.bid_levels[0].price.toString(2) == "101.00");
  CHECK(view.bid_levels[1].price.toString(2) == "100.00");
  CHECK(view.bid_levels[1].quantity.toString(0) == "4");
  CHECK(view.ask_levels.size() >= 2);
  CHECK(view.ask_levels[0].price.toString(2) == "105.00");
  CHECK(view.ask_levels[1].price.toString(2) == "106.00");
  CHECK(view.last_feed_timestamp_ns == 5000);
  CHECK(view.max_feed_timestamp_ns == 5000);
  CHECK(view.min_feed_timestamp_ns == 2000);
  engine.stop();
}

TEST_CASE("aggregator waits for expected exchanges before publishing") {
  hermeneutic::aggregator::AggregationEngine engine;
  engine.setExpectedExchanges({"ex1", "ex2"});
  engine.start();

  std::mutex mutex;
  std::condition_variable cv;
  std::vector<hermeneutic::common::AggregatedBookView> updates;

  auto id = engine.subscribe([&](const hermeneutic::common::AggregatedBookView& view) {
    {
      std::lock_guard<std::mutex> lock(mutex);
      updates.push_back(view);
    }
    cv.notify_one();
  });

  engine.push(makeNewOrder("ex1", 1, Side::Bid, "100.00", "1", 1));
  engine.push(makeNewOrder("ex1", 2, Side::Ask, "102.00", "1", 2));
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  {
    std::lock_guard<std::mutex> lock(mutex);
    CHECK(updates.empty());
  }

  engine.push(makeNewOrder("ex2", 3, Side::Bid, "101.00", "2", 3));
  {
    std::unique_lock<std::mutex> lock(mutex);
    CHECK(cv.wait_for(lock, std::chrono::milliseconds(200), [&] { return updates.size() >= 1; }));
    CHECK(updates.back().exchange_count == 2);
    CHECK(updates.back().best_bid.price.toString(2) == "101.00");
  }

  engine.push(makeNewOrder("ex1", 4, Side::Bid, "103.00", "3", 4));
  {
    std::unique_lock<std::mutex> lock(mutex);
    CHECK(cv.wait_for(lock, std::chrono::milliseconds(200), [&] { return updates.size() >= 2; }));
    CHECK(updates.back().best_bid.price.toString(2) == "103.00");
  }

  engine.unsubscribe(id);
  engine.stop();
}

TEST_CASE("aggregator publishes asynchronously even with slow subscribers") {
  hermeneutic::aggregator::AggregationEngine engine;
  engine.setExpectedExchanges({"slow"});
  engine.start();

  std::atomic<int> callback_count{0};
  auto id = engine.subscribe([&](const hermeneutic::common::AggregatedBookView&) {
    callback_count.fetch_add(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  });

  engine.push(makeNewOrder("slow", 1, Side::Bid, "100.00", "1", 1));
  engine.push(makeNewOrder("slow", 2, Side::Bid, "101.00", "1", 2));
  engine.push(makeNewOrder("slow", 3, Side::Bid, "102.00", "1", 3));

  // If publishing blocks ingestion we would only see the first price here.
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  auto latest = engine.latest();
  CHECK(latest.best_bid.price.toString(2) == "102.00");

  // Eventually every event should reach subscribers.
  auto wait_for_callbacks = [&]() {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    while (std::chrono::steady_clock::now() < deadline) {
      if (callback_count.load() >= 3) {
        return true;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return callback_count.load() >= 3;
  };
  CHECK(wait_for_callbacks());

  engine.unsubscribe(id);
  engine.stop();
}
