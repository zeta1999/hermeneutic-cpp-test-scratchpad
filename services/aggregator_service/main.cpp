#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <grpcpp/grpcpp.h>
#include <spdlog/spdlog.h>

#include "aggregator.grpc.pb.h"
#include "hermeneutic/aggregator/aggregator.hpp"
#include "hermeneutic/aggregator/config.hpp"
#include "hermeneutic/aggregator/grpc_service.hpp"
#include "hermeneutic/cex_type1/feed.hpp"
#include "hermeneutic/common/events.hpp"

namespace {
std::atomic<bool> g_running{true};

void handleSignal(int) {
  g_running = false;
}
}  // namespace

int main(int argc, char** argv) {
  std::string config_path = "config/aggregator.json";
  if (argc > 1) {
    config_path = argv[1];
  }

  try {
    auto config = hermeneutic::aggregator::loadAggregatorConfig(config_path);

    hermeneutic::aggregator::AggregationEngine engine;
    std::vector<std::string> expected;
    expected.reserve(config.feeds.size());
    for (const auto& feed : config.feeds) {
      expected.push_back(feed.name);
    }
    engine.setExpectedExchanges(expected);
    engine.start();

    hermeneutic::aggregator::AggregatorGrpcService service(engine, config.grpc.auth_token, config.symbol);
    const std::string server_address = config.grpc.listen_address + ":" + std::to_string(config.grpc.port);
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    auto server = builder.BuildAndStart();
    if (!server) {
      spdlog::error("Failed to start gRPC server on {}", server_address);
      return 1;
    }
    spdlog::info("gRPC server listening on {}", server_address);

    std::thread server_thread([&] { server->Wait(); });

    std::vector<std::unique_ptr<hermeneutic::cex_type1::ExchangeFeed>> feeds;
    feeds.reserve(config.feeds.size());
    for (const auto& feed_config : config.feeds) {
      hermeneutic::cex_type1::FeedOptions options{
          .exchange = feed_config.name,
          .url = feed_config.url,
          .auth_token = feed_config.auth_token,
      };
      auto feed = hermeneutic::cex_type1::makeWebSocketFeed(
          options, [&engine](hermeneutic::common::BookEvent event) { engine.push(std::move(event)); });
      feeds.push_back(std::move(feed));
    }

    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    for (auto& feed : feeds) {
      feed->start();
    }

    while (g_running.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    for (auto& feed : feeds) {
      feed->stop();
    }

    server->Shutdown();
    if (server_thread.joinable()) {
      server_thread.join();
    }
    engine.stop();
    spdlog::info("Aggregator service stopped");
  } catch (const std::exception& ex) {
    spdlog::error("Aggregator service failed: {}", ex.what());
    return 1;
  }
  return 0;
}
