#include "hermeneutic/aggregator/aggregator.hpp"
#include "hermeneutic/aggregator/config.hpp"
#include "hermeneutic/common/assert.hpp"

#include <algorithm>
#include <limits>
#include <map>
#include <string>
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
  std::vector<common::PriceLevel> filtered_asks;
  filtered_asks.reserve(raw_ask_levels.size());
  for (const auto& level : raw_ask_levels) {
    if (best_bid_price > kZero && level.price <= best_bid_price) {
      continue;
    }
    filtered_asks.push_back(level);
  }

  const auto default_quantity = (best_bid_quantity > kZero) ? best_bid_quantity : common::Decimal::fromInteger(1);
  if (!filtered_asks.empty()) {
    view.ask_levels = std::move(filtered_asks);
    view.best_ask = AggregatedQuote{view.ask_levels.front().price, view.ask_levels.front().quantity};
    last_best_ask_valid_ = true;
    last_best_ask_ = view.best_ask;
  } else {
    view.ask_levels.clear();
    if (last_best_ask_valid_) {
      view.best_ask = last_best_ask_;
      view.ask_levels.push_back({last_best_ask_.price, last_best_ask_.quantity});
    } else {
      view.best_ask = AggregatedQuote{kZero, kZero};
    }
  }

  view.last_feed_timestamp_ns = latest_feed_ns;
  view.last_local_timestamp_ns = latest_local_ns;
  view.min_feed_timestamp_ns = (min_feed_ns == std::numeric_limits<std::int64_t>::max()) ? 0 : min_feed_ns;
  view.max_feed_timestamp_ns = max_feed_ns;
  view.min_local_timestamp_ns = (min_local_ns == std::numeric_limits<std::int64_t>::max()) ? 0 : min_local_ns;
  view.max_local_timestamp_ns = max_local_ns;

  const auto publish_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(view.timestamp.time_since_epoch()).count();
  view.publish_timestamp_ns = publish_ns;
  validateAggregatedView(view);
  const auto feed_span = (view.min_feed_timestamp_ns > 0 && view.max_feed_timestamp_ns > 0)
                             ? (view.max_feed_timestamp_ns - view.min_feed_timestamp_ns)
                             : 0;
  const auto local_span = (view.min_local_timestamp_ns > 0 && view.max_local_timestamp_ns > 0)
                              ? (view.max_local_timestamp_ns - view.min_local_timestamp_ns)
                              : 0;
  const auto publish_delay = (view.max_feed_timestamp_ns > 0)
                                 ? (publish_ns - view.max_feed_timestamp_ns)
                                 : 0;
  maybeWarnOnStaleness(feed_span, local_span, publish_delay);

  return view;
}

void AggregationEngine::validateAggregatedView(const AggregatedBookView& view) const {
#if defined(HERMENEUTIC_ENABLE_DEBUG_ASSERTS) && HERMENEUTIC_ENABLE_DEBUG_ASSERTS
  const auto zero = common::Decimal::fromRaw(0);
  if (view.best_bid.quantity > zero) {
    HERMENEUTIC_ASSERT_DEBUG(view.best_bid.price >= zero, "best bid price negative");
  }
  if (view.best_ask.quantity > zero) {
    HERMENEUTIC_ASSERT_DEBUG(view.best_ask.price >= zero, "best ask price negative");
  }
  if (view.best_bid.quantity > zero && view.best_ask.quantity > zero) {
    HERMENEUTIC_ASSERT_DEBUG(view.best_ask.price > view.best_bid.price,
                             "best ask must exceed best bid");
  }

  if (!view.bid_levels.empty()) {
    HERMENEUTIC_ASSERT_DEBUG(view.bid_levels.front().price == view.best_bid.price,
                             "first bid level must match best bid");
    HERMENEUTIC_ASSERT_DEBUG(view.bid_levels.front().quantity == view.best_bid.quantity,
                             "first bid level quantity must match best bid");
  }
  auto prev_bid_price = view.best_bid.price;
  bool first = true;
  for (const auto& level : view.bid_levels) {
    HERMENEUTIC_ASSERT_DEBUG(level.price >= zero, "bid level price negative");
    HERMENEUTIC_ASSERT_DEBUG(level.quantity > zero, "bid level quantity non-positive");
    if (!first) {
      HERMENEUTIC_ASSERT_DEBUG(level.price < prev_bid_price, "bid levels not strictly descending");
    }
    HERMENEUTIC_ASSERT_DEBUG(level.price <= view.best_bid.price, "bid level exceeds best bid price");
    prev_bid_price = level.price;
    first = false;
  }

  if (!view.ask_levels.empty()) {
    HERMENEUTIC_ASSERT_DEBUG(view.ask_levels.front().price == view.best_ask.price,
                             "first ask level must match best ask");
    HERMENEUTIC_ASSERT_DEBUG(view.ask_levels.front().quantity == view.best_ask.quantity,
                             "first ask level quantity must match best ask");
  }
  auto prev_ask_price = view.best_ask.price;
  first = true;
  for (const auto& level : view.ask_levels) {
    HERMENEUTIC_ASSERT_DEBUG(level.price >= zero, "ask level price negative");
    HERMENEUTIC_ASSERT_DEBUG(level.quantity > zero, "ask level quantity non-positive");
    if (!first) {
      HERMENEUTIC_ASSERT_DEBUG(level.price > prev_ask_price, "ask levels not strictly ascending");
    }
    HERMENEUTIC_ASSERT_DEBUG(level.price >= view.best_ask.price, "ask level below best ask");
    if (view.best_bid.quantity > zero) {
      HERMENEUTIC_ASSERT_DEBUG(level.price > view.best_bid.price,
                               "ask level must exceed best bid price");
    }
    prev_ask_price = level.price;
    first = false;
  }
#endif
}

void AggregationEngine::maybeWarnOnStaleness(std::int64_t feed_span,
                                             std::int64_t local_span,
                                             std::int64_t publish_delay) const {
  constexpr std::int64_t kSpreadThresholdNs =
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(2)).count();
  constexpr std::int64_t kDelayThresholdNs =
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(5)).count();
  constexpr auto kMinWarningInterval = std::chrono::seconds(5);

  std::vector<std::string> reasons;
  if (feed_span > kSpreadThresholdNs) {
    reasons.push_back("feed window " + std::to_string(feed_span) + "ns");
  }
  if (local_span > kSpreadThresholdNs) {
    reasons.push_back("local window " + std::to_string(local_span) + "ns");
  }
  if (publish_delay > kDelayThresholdNs) {
    reasons.push_back("publish lag " + std::to_string(publish_delay) + "ns");
  }
  if (reasons.empty()) {
    return;
  }
  const auto now = std::chrono::steady_clock::now();
  if (now - last_staleness_warning_ < kMinWarningInterval) {
    return;
  }
  last_staleness_warning_ = now;
  std::string message = reasons.front();
  for (std::size_t i = 1; i < reasons.size(); ++i) {
    message.append("; ").append(reasons[i]);
  }
  spdlog::warn("Aggregated feed staleness detected: {}", message);
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
