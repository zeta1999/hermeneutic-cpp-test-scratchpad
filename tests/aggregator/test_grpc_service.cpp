#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <chrono>
#include <memory>
#include <thread>

#include <grpcpp/grpcpp.h>

#include "aggregator.grpc.pb.h"
#include "hermeneutic/aggregator/aggregator.hpp"
#include "hermeneutic/aggregator/grpc_service.hpp"
#include "hermeneutic/common/events.hpp"
#include "tests/support/test_data_factory.hpp"

using hermeneutic::aggregator::AggregationEngine;
using hermeneutic::aggregator::AggregatorGrpcService;
using hermeneutic::tests::support::makeNewOrder;
using hermeneutic::tests::support::timeFromNanoseconds;

namespace {
std::unique_ptr<grpc::Server> startServer(AggregatorGrpcService& service,
                                          std::shared_ptr<grpc::Channel>& channel) {
  grpc::ServerBuilder builder;
  builder.RegisterService(&service);
  auto server = builder.BuildAndStart();
  CHECK(static_cast<bool>(server));
  channel = server->InProcessChannel(grpc::ChannelArguments{});
  return server;
}

void shutdownServer(std::unique_ptr<grpc::Server>& server) {
  if (!server) {
    return;
  }
  server->Shutdown(std::chrono::system_clock::now());
  server->Wait();
  server.reset();
}

std::unique_ptr<hermeneutic::grpc::AggregatorService::Stub> makeStub(
    const std::shared_ptr<grpc::Channel>& channel) {
  return hermeneutic::grpc::AggregatorService::NewStub(channel);
}

}  // namespace

TEST_CASE("Aggregator gRPC service rejects missing tokens") {
  AggregationEngine engine;
  engine.start();
  AggregatorGrpcService service(engine, "secret-token", "BTCUSDT");
  std::shared_ptr<grpc::Channel> channel;
  auto server = startServer(service, channel);
  auto stub = makeStub(channel);

  grpc::ClientContext ctx;
  hermeneutic::grpc::SubscribeRequest request;
  hermeneutic::grpc::AggregatedBook book;
  auto reader = stub->StreamBooks(&ctx, request);
  CHECK(!reader->Read(&book));
  const auto status = reader->Finish();
  CHECK(status.error_code() == grpc::StatusCode::UNAUTHENTICATED);

  shutdownServer(server);
  engine.stop();
}

TEST_CASE("Aggregator gRPC service rejects unsupported symbols") {
  AggregationEngine engine;
  engine.start();
  AggregatorGrpcService service(engine, "secret-token", "BTCUSDT");
  std::shared_ptr<grpc::Channel> channel;
  auto server = startServer(service, channel);
  auto stub = makeStub(channel);

  grpc::ClientContext ctx;
  ctx.AddMetadata("authorization", "Bearer secret-token");
  hermeneutic::grpc::SubscribeRequest request;
  request.set_symbol("ETHUSDT");
  hermeneutic::grpc::AggregatedBook book;
  auto reader = stub->StreamBooks(&ctx, request);
  CHECK(!reader->Read(&book));
  const auto status = reader->Finish();
  CHECK(status.error_code() == grpc::StatusCode::INVALID_ARGUMENT);

  shutdownServer(server);
  engine.stop();
}

TEST_CASE("Aggregator gRPC service streams snapshots for authorized clients") {
  AggregationEngine engine;
  engine.start();
  AggregatorGrpcService service(engine, "secret-token", "BTCUSDT");
  std::shared_ptr<grpc::Channel> channel;
  auto server = startServer(service, channel);
  auto stub = makeStub(channel);

  grpc::ClientContext ctx;
  ctx.AddMetadata("authorization", "secret-token");
  hermeneutic::grpc::SubscribeRequest request;
  auto reader = stub->StreamBooks(&ctx, request);

  engine.push(makeNewOrder("ex1", 1, hermeneutic::common::Side::Bid, "100.00", "2", 1,
                           timeFromNanoseconds(10)));
  engine.push(makeNewOrder("ex2", 2, hermeneutic::common::Side::Ask, "101.00", "1", 2,
                           timeFromNanoseconds(20)));

  hermeneutic::grpc::AggregatedBook message;
  bool received = false;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
  while (std::chrono::steady_clock::now() < deadline) {
    if (reader->Read(&message)) {
      received = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  shutdownServer(server);
  const auto status = reader->Finish();
  CHECK(status.ok() || status.error_code() == grpc::StatusCode::CANCELLED ||
        status.error_code() == grpc::StatusCode::UNAVAILABLE);
  CHECK(received);
  if (received) {
    CHECK(!message.best_bid().price().empty());
    CHECK(!message.best_ask().price().empty());
    CHECK(message.exchange_count() >= 1);
  }

  engine.stop();
}
