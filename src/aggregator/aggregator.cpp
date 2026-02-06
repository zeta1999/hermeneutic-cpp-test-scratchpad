#include "hermeneutic/aggregator/aggregator.hpp"
#include "hermeneutic/aggregator/config.hpp"

#include <algorithm>
#include <spdlog/spdlog.h>
#include <simdjson.h>
#include <stdexcept>
#include <vector>

namespace hermeneutic::aggregator {

using common::AggregatedBookView;
using common::AggregatedQuote;
using common::Decimal;
using common::MarketUpdate;

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
  worker_ = std::thread(&AggregationEngine::run, this);
}

void AggregationEngine::stop() {
  running_.store(false);
  queue_.close();
  if (worker_.joinable()) {
    worker_.join();
  }
}

void AggregationEngine::push(MarketUpdate update) {
  queue_.push(std::move(update));
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

void AggregationEngine::run() {
  MarketUpdate update;
  while (running_.load()) {
    if (!queue_.wait_pop(update)) {
      break;
    }

    AggregatedBookView snapshot;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      auto& book = books_[update.exchange];
      book.apply(update);
      view_ = consolidate();
      snapshot = view_;
    }
    publish(snapshot);
  }
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

  bool has_bid = false;
  bool has_ask = false;
  Decimal best_bid_price = kZero;
  Decimal best_ask_price = kZero;
  Decimal best_bid_qty = kZero;
  Decimal best_ask_qty = kZero;

  for (const auto& [name, book] : books_) {
    (void)name;
    auto bid = book.bestBid();
    if (bid.quantity > kZero) {
      if (!has_bid || bid.price > best_bid_price) {
        best_bid_price = bid.price;
        best_bid_qty = bid.quantity;
        has_bid = true;
      } else if (bid.price == best_bid_price) {
        best_bid_qty += bid.quantity;
      }
    }

    auto ask = book.bestAsk();
    if (ask.quantity > kZero) {
      if (!has_ask || ask.price < best_ask_price) {
        best_ask_price = ask.price;
        best_ask_qty = ask.quantity;
        has_ask = true;
      } else if (ask.price == best_ask_price) {
        best_ask_qty += ask.quantity;
      }
    }
  }

  if (!has_ask) {
    best_ask_price = kZero;
    best_ask_qty = kZero;
  }
  if (!has_bid) {
    best_bid_price = kZero;
    best_bid_qty = kZero;
  }

  view.best_bid = AggregatedQuote{best_bid_price, best_bid_qty};
  view.best_ask = AggregatedQuote{best_ask_price, best_ask_qty};
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
    if (auto interval = feed_obj["interval_ms"].get_uint64(); interval.error() == simdjson::SUCCESS) {
      feed.interval = std::chrono::milliseconds(interval.value());
    }
    config.feeds.push_back(std::move(feed));
  }
  return config;
}

}  // namespace hermeneutic::aggregator
