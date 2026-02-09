#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace hermeneutic::aggregator {

struct FeedConfig {
  std::string name;
  std::string url;
  std::string auth_token;
};

struct GrpcConfig {
  std::string listen_address{"0.0.0.0"};
  int port{50051};
  std::string auth_token;
};

struct AggregatorConfig {
  std::vector<FeedConfig> feeds;
  std::chrono::milliseconds publish_interval{50};
  std::string symbol{"BTCUSDT"};
  GrpcConfig grpc;
};

AggregatorConfig loadAggregatorConfig(const std::string& path);

}  // namespace hermeneutic::aggregator
