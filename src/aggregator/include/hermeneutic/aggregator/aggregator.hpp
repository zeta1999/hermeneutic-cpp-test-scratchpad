#pragma once

#include <atomic>
#include <cstddef>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>

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

 private:
  void run();
  void publish(const common::AggregatedBookView& view);
  common::AggregatedBookView consolidate() const;

  mutable std::mutex mutex_;
  std::unordered_map<std::string, lob::LimitOrderBook> books_;
  common::AggregatedBookView view_{};

  common::ConcurrentQueue<common::BookEvent> queue_;
  std::thread worker_;
  std::atomic<bool> running_{false};

  std::unordered_map<SubscriberId, Subscriber> subscribers_;
  std::atomic<SubscriberId> next_subscriber_id_{1};
};

}  // namespace hermeneutic::aggregator
