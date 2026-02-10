#include "hermeneutic/aggregator/aggregator.hpp"
#include "hermeneutic/aggregator/config.hpp"

#include <algorithm>
#include <limits>
#include <map>
#include <spdlog/spdlog.h>
#include <simdjson.h>
#include <stdexcept>
#include <vector>

namespace hermeneutic::aggregator {

using common::AggregatedBookView;
using common::AggregatedQuote;
using common::BookEvent;
using common::Decimal;

namespace {
constexpr Decimal kZero = Decimal::fromRaw(0);
}

AggregationEngine::AggregationEngine() = default;

AggregationEngine::~AggregationEngine() {
  stop();
}

void AggregationEngine::start() {
  if (running_.exchange(true)) {
    return;
  }
  publisher_ = std::thread(&AggregationEngine::publisherLoop, this);
  worker_ = std::thread(&AggregationEngine::run, this);
}

void AggregationEngine::stop() {
  running_.store(false);
  queue_.close();
  if (worker_.joinable()) {
    worker_.join();
  }
  publish_queue_.close();
  if (publisher_.joinable()) {
    publisher_.join();
  }
}

void AggregationEngine::push(BookEvent event) {
  queue_.push(std::move(event));
}

AggregationEngine::SubscriberId AggregationEngine::subscribe(Subscriber subscriber) {
  const auto id = next_subscriber_id_.fetch_add(1);
  std::lock_guard<std::mutex> lock(mutex_);
  subscribers_.emplace(id, std::move(subscriber));
  return id;
}

void AggregationEngine::unsubscribe(SubscriberId id) {
  std::lock_guard<std::mutex> lock(mutex_);
  subscribers_.erase(id);
}

common::AggregatedBookView AggregationEngine::latest() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return view_;
}

void AggregationEngine::setExpectedExchanges(std::vector<std::string> exchanges) {
  std::lock_guard<std::mutex> lock(mutex_);
  expected_exchanges_.clear();
  ready_exchanges_.clear();
  for (auto& ex : exchanges) {
    expected_exchanges_.insert(ex);
  }
  require_all_ready_ = !expected_exchanges_.empty();
}

void AggregationEngine::run() {
  BookEvent update;
  while (running_.load()) {
    if (!queue_.wait_pop(update)) {
      break;
    }

    AggregatedBookView snapshot;
    bool can_publish = true;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      auto& book = books_[update.exchange];
      book.apply(update);
      if (require_all_ready_ && expected_exchanges_.count(update.exchange)) {
        ready_exchanges_.insert(update.exchange);
      }
      view_ = consolidate();
      snapshot = view_;
      can_publish = !require_all_ready_ || ready_exchanges_.size() == expected_exchanges_.size();
    }
    if (can_publish) {
      enqueueSnapshot(std::move(snapshot));
    }
  }
}

void AggregationEngine::publisherLoop() {
  AggregatedBookView snapshot;
  while (publish_queue_.wait_pop(snapshot)) {
    publish(snapshot);
  }
}

void AggregationEngine::enqueueSnapshot(AggregatedBookView view) {
  publish_queue_.push(std::move(view));
}

void AggregationEngine::publish(const AggregatedBookView& view) {
  std::vector<Subscriber> subscribers;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    subscribers.reserve(subscribers_.size());
    for (const auto& [_, cb] : subscribers_) {
      subscribers.push_back(cb);
    }
  }
  for (auto& subscriber : subscribers) {
    if (subscriber) {
      subscriber(view);
    }
  }
}

AggregatedBookView AggregationEngine::consolidate() const {
  AggregatedBookView view;
  view.exchange_count = books_.size();
  view.timestamp = std::chrono::system_clock::now();

  std::map<Decimal, Decimal, std::greater<Decimal>> aggregated_bids;
  std::map<Decimal, Decimal, std::less<Decimal>> aggregated_asks;

  std::int64_t latest_feed_ns = 0;
  std::int64_t latest_local_ns = 0;
  std::int64_t min_feed_ns = std::numeric_limits<std::int64_t>::max();
  std::int64_t max_feed_ns = 0;
  std::int64_t min_local_ns = std::numeric_limits<std::int64_t>::max();
  std::int64_t max_local_ns = 0;

  for (const auto& [name, book] : books_) {
    (void)name;
    for (auto it = book.bidLevelsBegin(); it != book.bidLevelsEnd(); ++it) {
      aggregated_bids[it->first] += it->second;
    }
    for (auto it = book.askLevelsBegin(); it != book.askLevelsEnd(); ++it) {
      aggregated_asks[it->first] += it->second;
    }

    const auto feed_ns = book.lastFeedTimestampNs();
    if (feed_ns > 0) {
      latest_feed_ns = std::max(latest_feed_ns, feed_ns);
      min_feed_ns = std::min(min_feed_ns, feed_ns);
      max_feed_ns = std::max(max_feed_ns, feed_ns);
    }

    const auto local_ns = book.lastLocalUpdateTimestampNs();
    if (local_ns > 0) {
      latest_local_ns = std::max(latest_local_ns, local_ns);
      min_local_ns = std::min(min_local_ns, local_ns);
      max_local_ns = std::max(max_local_ns, local_ns);
    }
  }

  view.bid_levels.reserve(aggregated_bids.size());
  for (const auto& [price, qty] : aggregated_bids) {
    if (qty > kZero) {
      view.bid_levels.push_back({price, qty});
    }
  }

  std::vector<common::PriceLevel> raw_ask_levels;
  raw_ask_levels.reserve(aggregated_asks.size());
  for (const auto& [price, qty] : aggregated_asks) {
    if (qty > kZero) {
      raw_ask_levels.push_back({price, qty});
    }
  }

  common::Decimal best_bid_price = kZero;
  common::Decimal best_bid_quantity = kZero;
  if (!view.bid_levels.empty()) {
    best_bid_price = view.bid_levels.front().price;
    best_bid_quantity = view.bid_levels.front().quantity;
    view.best_bid = AggregatedQuote{best_bid_price, best_bid_quantity};
  } else {
    view.best_bid = AggregatedQuote{kZero, kZero};
  }
  view.ask_levels.reserve(raw_ask_levels.size());
  for (const auto& level : raw_ask_levels) {
    if (best_bid_price > kZero && level.price <= best_bid_price) {
      continue;
    }
    view.ask_levels.push_back(level);
  }

  const auto default_quantity = (best_bid_quantity > kZero) ? best_bid_quantity : common::Decimal::fromInteger(1);
  const auto placeholder_price = best_bid_price > kZero
                                     ? best_bid_price * common::Decimal::fromDouble(1.0001)
                                     : common::Decimal::fromInteger(1);

  if (view.ask_levels.empty()) {
    view.ask_levels.push_back({placeholder_price, default_quantity});
    view.best_ask = AggregatedQuote{placeholder_price, default_quantity};
  } else {
    view.best_ask = AggregatedQuote{view.ask_levels.front().price, view.ask_levels.front().quantity};
    if (view.best_bid.price > kZero && view.best_ask.price <= view.best_bid.price) {
      view.ask_levels.front().price = placeholder_price;
      view.ask_levels.front().quantity = default_quantity;
      view.best_ask = AggregatedQuote{placeholder_price, default_quantity};
    }
  }

  view.last_feed_timestamp_ns = latest_feed_ns;
  view.last_local_timestamp_ns = latest_local_ns;
  view.min_feed_timestamp_ns = (min_feed_ns == std::numeric_limits<std::int64_t>::max()) ? 0 : min_feed_ns;
  view.max_feed_timestamp_ns = max_feed_ns;
  view.min_local_timestamp_ns = (min_local_ns == std::numeric_limits<std::int64_t>::max()) ? 0 : min_local_ns;
  view.max_local_timestamp_ns = max_local_ns;

  return view;
}

}  // namespace hermeneutic::aggregator

namespace hermeneutic::aggregator {

AggregatorConfig loadAggregatorConfig(const std::string& path) {
  simdjson::dom::parser parser;
  auto doc = parser.load(path);
  AggregatorConfig config;

  auto obj = doc.get_object();
  if (auto publish = obj["publish_interval_ms"].get_uint64(); publish.error() == simdjson::SUCCESS) {
    config.publish_interval = std::chrono::milliseconds(publish.value());
  }
  if (auto symbol = obj["symbol"].get_string(); symbol.error() == simdjson::SUCCESS) {
    config.symbol = std::string(symbol.value());
  }

  if (auto grpc_value = obj["grpc"].get_object(); grpc_value.error() == simdjson::SUCCESS) {
    if (auto listen = grpc_value["listen_address"].get_string(); listen.error() == simdjson::SUCCESS) {
      config.grpc.listen_address = std::string(listen.value());
    }
    if (auto port = grpc_value["port"].get_int64(); port.error() == simdjson::SUCCESS) {
      config.grpc.port = static_cast<int>(port.value());
    }
    if (auto token = grpc_value["auth_token"].get_string(); token.error() == simdjson::SUCCESS) {
      config.grpc.auth_token = std::string(token.value());
    }
  } else {
    throw std::runtime_error("config missing grpc section");
  }

  auto feeds = obj["feeds"].get_array();
  if (feeds.error() != simdjson::SUCCESS) {
    throw std::runtime_error("config missing feeds array");
  }
  for (auto feed_value : feeds.value()) {
    FeedConfig feed;
    auto feed_obj = feed_value.get_object();
    feed.name = std::string(feed_obj["name"].get_string().value());
    feed.url = std::string(feed_obj["url"].get_string().value());
    if (auto token = feed_obj["auth_token"].get_string(); token.error() == simdjson::SUCCESS) {
      feed.auth_token = std::string(token.value());
    }
    config.feeds.push_back(std::move(feed));
  }
  return config;
}

}  // namespace hermeneutic::aggregator
