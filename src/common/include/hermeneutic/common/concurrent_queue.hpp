#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <utility>

namespace hermeneutic::common {

template <typename T>
class ConcurrentQueue {
 public:
  void push(T value) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (closed_) {
        return;
      }
      queue_.push(std::move(value));
    }
    cond_var_.notify_one();
  }

  bool try_pop(T& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) {
      return false;
    }
    value = std::move(queue_.front());
    queue_.pop();
    return true;
  }

  bool wait_pop(T& value) {
    std::unique_lock<std::mutex> lock(mutex_);
    cond_var_.wait(lock, [this] { return closed_ || !queue_.empty(); });
    if (queue_.empty()) {
      return false;
    }
    value = std::move(queue_.front());
    queue_.pop();
    return true;
  }

  template <typename Rep, typename Period>
  bool wait_pop_for(T& value, const std::chrono::duration<Rep, Period>& timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!cond_var_.wait_for(lock, timeout, [this] { return closed_ || !queue_.empty(); })) {
      return false;
    }
    if (queue_.empty()) {
      return false;
    }
    value = std::move(queue_.front());
    queue_.pop();
    return true;
  }

  void close() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      closed_ = true;
    }
    cond_var_.notify_all();
  }

 private:
  std::queue<T> queue_;
  std::mutex mutex_;
  std::condition_variable cond_var_;
  bool closed_{false};
};

}  // namespace hermeneutic::common
