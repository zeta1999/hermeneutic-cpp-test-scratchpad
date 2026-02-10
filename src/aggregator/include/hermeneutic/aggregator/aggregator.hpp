#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include "hermeneutic/common/assert.hpp"
#include "hermeneutic/common/concurrent_queue.hpp"
#include "hermeneutic/common/events.hpp"
#include "hermeneutic/lob/order_book.hpp"

namespace hermeneutic::aggregator {

class AggregationEngine {
 public:
  using SubscriberId = std::size_t;
  using Subscriber = std::function<void(const common::AggregatedBookView&)>;

  AggregationEngine();
  ~AggregationEngine();

  void start();
  void stop();

  void push(common::BookEvent event);
  SubscriberId subscribe(Subscriber subscriber);
  void unsubscribe(SubscriberId id);

  common::AggregatedBookView latest() const;
  void setExpectedExchanges(std::vector<std::string> exchanges);

 private:
  void run();
  void publisherLoop();
  void enqueueSnapshot(common::AggregatedBookView view);
  void publish(const common::AggregatedBookView& view);
  common::AggregatedBookView consolidate() const;
  void validateAggregatedView(const common::AggregatedBookView& view) const;
  void maybeWarnOnStaleness(std::int64_t feed_span,
                            std::int64_t local_span,
                            std::int64_t publish_delay) const;

  mutable std::mutex mutex_;
  std::unordered_map<std::string, lob::LimitOrderBook> books_;
  common::AggregatedBookView view_{};
  mutable common::AggregatedQuote last_best_ask_{};
  mutable bool last_best_ask_valid_{false};
  std::unordered_set<std::string> expected_exchanges_;
  std::unordered_set<std::string> ready_exchanges_;
  bool require_all_ready_{false};

  common::ConcurrentQueue<common::BookEvent> queue_;
  common::ConcurrentQueue<common::AggregatedBookView> publish_queue_;
  std::thread worker_;
  std::thread publisher_;
  std::atomic<bool> running_{false};
  mutable std::chrono::steady_clock::time_point last_staleness_warning_{};

  std::unordered_map<SubscriberId, Subscriber> subscribers_;
  std::atomic<SubscriberId> next_subscriber_id_{1};
};

}  // namespace hermeneutic::aggregator
