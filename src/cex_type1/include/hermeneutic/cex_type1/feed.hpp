#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <string>

#include "hermeneutic/common/events.hpp"

namespace hermeneutic::cex_type1 {

struct FeedOptions {
  std::string exchange;
  std::string url;
  std::string auth_token;
  std::chrono::milliseconds interval{0};
};

class ExchangeFeed {
 public:
  using Callback = std::function<void(common::BookEvent)>;
  virtual ~ExchangeFeed() = default;

  virtual void start() = 0;
  virtual void stop() = 0;
};

std::unique_ptr<ExchangeFeed> makeWebSocketFeed(FeedOptions options, ExchangeFeed::Callback callback);

}  // namespace hermeneutic::cex_type1
