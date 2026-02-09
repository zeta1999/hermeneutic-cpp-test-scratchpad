#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#ifndef REQUIRE
#define REQUIRE(...) CHECK(__VA_ARGS__)
#endif

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <grpcpp/grpcpp.h>

#include <spdlog/spdlog.h>

#include "hermeneutic/aggregator/aggregator.hpp"
#include "hermeneutic/aggregator/grpc_service.hpp"
#include "hermeneutic/common/events.hpp"
#include "common/book_stream_client.hpp"

namespace {
using hermeneutic::common::BookEvent;
using hermeneutic::common::BookEventKind;
using hermeneutic::common::Decimal;
using hermeneutic::common::Side;

BookEvent makeNewOrder(std::string exchange,
                       std::uint64_t id,
                       Side side,
                       std::string price,
                       std::string quantity,
                       std::uint64_t sequence) {
  BookEvent event;
  event.exchange = std::move(exchange);
  event.kind = BookEventKind::NewOrder;
  event.sequence = sequence;
  event.order.order_id = id;
  event.order.side = side;
  event.order.price = Decimal::fromString(price);
  event.order.quantity = Decimal::fromString(quantity);
  return event;
}

std::unique_ptr<grpc::Server> startServer(hermeneutic::aggregator::AggregatorGrpcService& service) {
  spdlog::info("Starting in-process gRPC server");
  grpc::ServerBuilder builder;
  builder.RegisterService(&service);
  auto server = builder.BuildAndStart();
  REQUIRE(server != nullptr);
  spdlog::info("gRPC server ready (in-process)");
  return server;
}

}  // namespace

TEST_CASE("book stream client reconnects when server restarts") {
  spdlog::info("[test_book_stream_client] Starting test case");
  hermeneutic::aggregator::AggregationEngine engine;
  engine.start();
  spdlog::info("Aggregation engine started");
  hermeneutic::aggregator::AggregatorGrpcService service(engine, "", "BTCUSDT");

  std::mutex server_mutex;
  grpc::Server* server_raw = nullptr;
  auto channel_factory = [&]() -> std::shared_ptr<grpc::Channel> {
    std::lock_guard<std::mutex> lock(server_mutex);
    if (server_raw == nullptr) {
      return nullptr;
    }
    return server_raw->InProcessChannel(grpc::ChannelArguments{});
  };

  auto server = startServer(service);
  {
    std::lock_guard<std::mutex> lock(server_mutex);
    server_raw = server.get();
  }
  spdlog::info("Server initialized in-process");

  std::mutex mutex;
  std::condition_variable cv;
  std::vector<hermeneutic::common::AggregatedBookView> snapshots;

  hermeneutic::services::BookStreamClient client(
      channel_factory,
      "",
      "BTCUSDT",
      [&](const hermeneutic::common::AggregatedBookView& view) {
        std::lock_guard<std::mutex> lock(mutex);
        snapshots.push_back(view);
        cv.notify_all();
      },
      std::chrono::milliseconds(100));
  client.start();
  spdlog::info("BookStreamClient started using in-process channel");
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  engine.push(makeNewOrder("s1", 1, Side::Bid, "100.00", "1", 1));
  engine.push(makeNewOrder("s2", 2, Side::Ask, "101.00", "2", 2));
  spdlog::info("Injected initial events");

  {
    std::unique_lock<std::mutex> lock(mutex);
    spdlog::info("Waiting for initial snapshot");
    REQUIRE(cv.wait_for(lock, std::chrono::seconds(1), [&] { return snapshots.size() >= 1; }));
    spdlog::info("Received initial snapshot count {}", snapshots.size());
  }

  {
    std::lock_guard<std::mutex> lock(server_mutex);
    server_raw = nullptr;
  }
  server->Shutdown();
  server->Wait();
  server.reset();
  spdlog::info("Shut down server to simulate restart");

  engine.push(makeNewOrder("s1", 3, Side::Bid, "102.00", "1", 3));
  spdlog::info("Injected event while server offline");

  server = startServer(service);
  {
    std::lock_guard<std::mutex> lock(server_mutex);
    server_raw = server.get();
  }
  spdlog::info("Server restarted in-process");
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  engine.push(makeNewOrder("s2", 4, Side::Ask, "103.00", "1", 4));
  spdlog::info("Injected final event");

  {
    std::unique_lock<std::mutex> lock(mutex);
    spdlog::info("Waiting for snapshot after restart");
    REQUIRE(cv.wait_for(lock, std::chrono::seconds(2), [&] { return snapshots.size() >= 2; }));
    spdlog::info("Snapshots after restart: {}", snapshots.size());
  }

  client.stop();
  {
    std::lock_guard<std::mutex> lock(server_mutex);
    server_raw = nullptr;
  }
  server->Shutdown();
  server->Wait();
  engine.stop();
  spdlog::info("Stopped client and engine");

  REQUIRE(snapshots.size() >= 2);
  CHECK(snapshots.back().best_ask.price >= snapshots.front().best_ask.price);
  spdlog::info("[test_book_stream_client] Completed successfully with {} snapshots", snapshots.size());
}
