#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <grpcpp/grpcpp.h>

#include "aggregator.grpc.pb.h"
#include "hermeneutic/common/assert.hpp"
#include "hermeneutic/common/events.hpp"

namespace hermeneutic::services {

class BookStreamClient {
 public:
  using Callback = std::function<void(const hermeneutic::common::AggregatedBookView&)>;
  using ChannelFactory = std::function<std::shared_ptr<::grpc::Channel>()>;

  BookStreamClient(std::string endpoint,
                   std::string token,
                   std::string symbol,
                   Callback callback,
                   std::chrono::milliseconds reconnect_delay = std::chrono::milliseconds(500));
  BookStreamClient(ChannelFactory channel_factory,
                   std::string token,
                   std::string symbol,
                   Callback callback,
                   std::chrono::milliseconds reconnect_delay = std::chrono::milliseconds(500));
  ~BookStreamClient();

  void start();
  void stop();

 private:
  void run();
  void cancelActiveContext();

  std::string endpoint_;
  ChannelFactory channel_factory_;
  std::string token_;
  std::string symbol_;
  Callback callback_;
  std::chrono::milliseconds reconnect_delay_;
  std::atomic<bool> running_{false};
  std::thread worker_;
  std::mutex context_mutex_;
  ::grpc::ClientContext* active_context_{nullptr};
};

}  // namespace hermeneutic::services
