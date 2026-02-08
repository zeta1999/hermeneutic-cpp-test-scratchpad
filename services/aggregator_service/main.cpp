#include <atomic>
#include <chrono>
#include <csignal>
#include <deque>
#include <cstdint>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <grpcpp/grpcpp.h>
#include <spdlog/spdlog.h>

#include "aggregator.grpc.pb.h"
#include "hermeneutic/aggregator/aggregator.hpp"
#include "hermeneutic/aggregator/config.hpp"
#include "hermeneutic/cex_type1/feed.hpp"
#include "hermeneutic/common/events.hpp"
#include "services/common/grpc_helpers.hpp"

namespace {
std::atomic<bool> g_running{true};

void handleSignal(int) {
  g_running = false;
}

class SubscriptionGuard {
 public:
  SubscriptionGuard(hermeneutic::aggregator::AggregationEngine& engine,
                    hermeneutic::aggregator::AggregationEngine::SubscriberId id)
      : engine_(engine), id_(id) {}
  ~SubscriptionGuard() { engine_.unsubscribe(id_); }

 private:
  hermeneutic::aggregator::AggregationEngine& engine_;
  hermeneutic::aggregator::AggregationEngine::SubscriberId id_;
};

class AggregatorGrpcService final : public hermeneutic::grpc::AggregatorService::Service {
 public:
  AggregatorGrpcService(hermeneutic::aggregator::AggregationEngine& engine,
                        std::string token,
                        std::string symbol)
      : engine_(engine), expected_token_(std::move(token)), symbol_(std::move(symbol)) {}

  grpc::Status StreamBooks(::grpc::ServerContext* context,
                           const hermeneutic::grpc::SubscribeRequest* request,
                           ::grpc::ServerWriter<hermeneutic::grpc::AggregatedBook>* writer) override {
    if (!authorize(*context)) {
      return {grpc::StatusCode::UNAUTHENTICATED, "missing or invalid token"};
    }
    if (!request->symbol().empty() && request->symbol() != symbol_) {
      return {grpc::StatusCode::INVALID_ARGUMENT, "unsupported symbol"};
    }

    std::mutex mutex;
    std::condition_variable cv;
    std::deque<hermeneutic::common::AggregatedBookView> queue;

    auto subscriber_id = engine_.subscribe([&](const hermeneutic::common::AggregatedBookView& view) {
      {
        std::lock_guard<std::mutex> lock(mutex);
        queue.push_back(view);
      }
      cv.notify_one();
    });
    SubscriptionGuard guard(engine_, subscriber_id);

    while (g_running.load()) {
      hermeneutic::common::AggregatedBookView view;
      {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [&] { return !queue.empty() || context->IsCancelled() || !g_running.load(); });
        if (context->IsCancelled() || !g_running.load()) {
          break;
        }
        view = queue.front();
        queue.pop_front();
      }
      if (!writer->Write(hermeneutic::services::grpc_helpers::FromDomain(view))) {
        break;
      }
    }
    return grpc::Status::OK;
  }

 private:
  bool authorize(const grpc::ServerContext& context) const {
    if (expected_token_.empty()) {
      return true;
    }
    const auto& metadata = context.client_metadata();
    auto it = metadata.find("authorization");
    if (it == metadata.end()) {
      return false;
    }
    std::string header(it->second.data(), it->second.length());
    const std::string prefix = "Bearer ";
    if (header.rfind(prefix, 0) == 0) {
      header.erase(0, prefix.size());
    }
    return header == expected_token_;
  }

  hermeneutic::aggregator::AggregationEngine& engine_;
  std::string expected_token_;
  std::string symbol_;
};

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

    AggregatorGrpcService service(engine, config.grpc.auth_token, config.symbol);
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
          .interval = feed_config.interval,
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
