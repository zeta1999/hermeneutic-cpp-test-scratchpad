#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#ifndef REQUIRE
#define REQUIRE(...) CHECK(__VA_ARGS__)
#endif

#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/Net/StreamSocket.h>
#include <Poco/Process.h>

#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <grpcpp/grpcpp.h>

#include "services/common/grpc_helpers.hpp"
#include "hermeneutic/common/events.hpp"

namespace {

bool canBindLoopback() {
  try {
    Poco::Net::ServerSocket probe(Poco::Net::SocketAddress("127.0.0.1", 0));
    (void)probe.address();
    return true;
  } catch (const Poco::Exception& ex) {
    std::cerr << "Skipping aggregator_service test: " << ex.displayText() << std::endl;
    return false;
  }
}

unsigned short reservePort() {
  Poco::Net::ServerSocket socket(Poco::Net::SocketAddress("127.0.0.1", 0));
  return socket.address().port();
}

std::filesystem::path aggregatorBinary() {
  return std::filesystem::path(PROJECT_BINARY_DIR) / "services/aggregator_service/aggregator_service";
}

std::filesystem::path cexBinary() {
  return std::filesystem::path(PROJECT_BINARY_DIR) / "services/cex_type1_service/cex_type1_service";
}

bool waitForTcp(const std::string& host, unsigned short port, std::chrono::milliseconds timeout) {
  auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    try {
      Poco::Net::StreamSocket probe;
      probe.connect(Poco::Net::SocketAddress(host, port));
      probe.close();
      return true;
    } catch (...) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
  }
  return false;
}

}  // namespace

TEST_CASE("aggregator_service streams aggregated books from cex executables") {
  using namespace std::chrono_literals;
  if (!canBindLoopback()) {
    return;
  }
  auto agg_bin = aggregatorBinary();
  auto cex_bin = cexBinary();
  if (!std::filesystem::exists(agg_bin) || !std::filesystem::exists(cex_bin)) {
    std::cerr << "Skipping aggregator_service test: binaries missing" << std::endl;
    return;
  }

  std::signal(SIGPIPE, SIG_IGN);

  auto temp_dir = std::filesystem::temp_directory_path() / "aggregator_service_test";
  std::filesystem::create_directories(temp_dir);

  struct FeedProcess {
    std::unique_ptr<Poco::ProcessHandle> handle;
    std::filesystem::path file;
    unsigned short port;
  };

  std::vector<FeedProcess> feeds;
  for (int i = 0; i < 2; ++i) {
    FeedProcess proc;
    proc.port = reservePort();
    proc.file = temp_dir / ("feed" + std::to_string(i) + ".ndjson");
    std::ofstream out(proc.file);
    out << "{\"type\":\"new_order\",\"sequence\":1,\"order_id\":" << (100 + i)
        << ",\"side\":\"bid\",\"price\":\"" << (100 + i) << ".00\",\"quantity\":\"1\"}" << std::endl;
    out << "{\"type\":\"new_order\",\"sequence\":2,\"order_id\":" << (200 + i)
        << ",\"side\":\"ask\",\"price\":\"" << (101 + i) << ".00\",\"quantity\":\"2\"}" << std::endl;
    out.close();

    std::vector<std::string> args = {
        "feed" + std::to_string(i),
        proc.file.string(),
        std::to_string(proc.port),
        "token" + std::to_string(i),
        "10"};
    try {
      proc.handle = std::make_unique<Poco::ProcessHandle>(Poco::Process::launch(cex_bin.string(), args));
    } catch (const Poco::Exception& ex) {
      std::cerr << "Skipping aggregator_service test: " << ex.displayText() << std::endl;
      return;
    }
    REQUIRE(waitForTcp("127.0.0.1", proc.port, 2s));
    feeds.push_back(std::move(proc));
  }

  auto config_path = temp_dir / "aggregator.json";
  unsigned short grpc_port = reservePort();
  {
    std::ofstream cfg(config_path);
    cfg << "{\n";
    cfg << "  \"symbol\": \"BTCUSDT\",\n";
    cfg << "  \"grpc\": {\n";
    cfg << "    \"listen_address\": \"127.0.0.1\",\n";
    cfg << "    \"port\": " << grpc_port << ",\n";
    cfg << "    \"auth_token\": \"agg-token\"\n";
    cfg << "  },\n";
    cfg << "  \"feeds\": [\n";
    for (std::size_t i = 0; i < feeds.size(); ++i) {
      cfg << "    {\n";
      cfg << "      \"name\": \"feed" << i << "\",\n";
      cfg << "      \"url\": \"ws://127.0.0.1:" << feeds[i].port << "/feed" << i << "\",\n";
      cfg << "      \"auth_token\": \"token" << i << "\"\n";
      cfg << "    }" << (i + 1 == feeds.size() ? "\n" : ",\n");
    }
    cfg << "  ]\n";
    cfg << "}\n";
  }

  std::unique_ptr<Poco::ProcessHandle> agg_handle;

  auto cleanup = [&]() {
    if (agg_handle) {
      try {
        Poco::Process::kill(*agg_handle);
        agg_handle->wait();
      } catch (...) {
      }
    }
    for (auto& proc : feeds) {
      if (proc.handle) {
        try {
          Poco::Process::kill(*proc.handle);
          proc.handle->wait();
        } catch (...) {
        }
      }
      std::error_code ec;
      std::filesystem::remove(proc.file, ec);
    }
    std::error_code ec;
    std::filesystem::remove(config_path, ec);
    std::filesystem::remove(temp_dir, ec);
  };

  try {
    agg_handle = std::make_unique<Poco::ProcessHandle>(
        Poco::Process::launch(agg_bin.string(), std::vector<std::string>{config_path.string()}));
  } catch (const Poco::Exception& ex) {
    std::cerr << "Failed to start aggregator_service: " << ex.displayText() << std::endl;
    cleanup();
    CHECK(false);
    return;
  }

  auto channel = grpc::CreateChannel("127.0.0.1:" + std::to_string(grpc_port), grpc::InsecureChannelCredentials());
  if (!channel->WaitForConnected(std::chrono::system_clock::now() + 2s)) {
    cleanup();
    CHECK(false);
    return;
  }
  auto stub = hermeneutic::grpc::AggregatorService::NewStub(channel);

  grpc::ClientContext context;
  hermeneutic::services::grpc_helpers::AttachAuth(context, "agg-token");
  hermeneutic::grpc::SubscribeRequest request;
  request.set_symbol("BTCUSDT");
  auto reader = stub->StreamBooks(&context, request);
  hermeneutic::grpc::AggregatedBook message;

  bool received = reader->Read(&message);
  context.TryCancel();
  reader->Finish();
  cleanup();

  REQUIRE(received);
  auto view = hermeneutic::services::grpc_helpers::ToDomain(message);
  CHECK(view.exchange_count == 2);
  CHECK(view.best_bid.price >= hermeneutic::common::Decimal::fromString("100.00"));
  CHECK(view.best_ask.price >= hermeneutic::common::Decimal::fromString("101.00"));
  CHECK(view.last_feed_timestamp_ns > 0);
  CHECK(view.last_local_timestamp_ns > 0);
  CHECK(view.min_feed_timestamp_ns > 0);
  CHECK(view.min_local_timestamp_ns > 0);
  CHECK(view.timestamp.time_since_epoch().count() > 0);
}
