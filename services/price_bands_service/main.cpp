#include <grpcpp/grpcpp.h>
#include <spdlog/spdlog.h>

#include <string>

#include "aggregator.grpc.pb.h"
#include "hermeneutic/price_bands/price_bands_publisher.hpp"
#include "services/common/grpc_helpers.hpp"

int main(int argc, char** argv) {
  std::string endpoint = "127.0.0.1:50051";
  std::string token = "";
  std::string symbol = "BTCUSDT";
  if (argc > 1) {
    endpoint = argv[1];
  }
  if (argc > 2) {
    token = argv[2];
  }
  if (argc > 3) {
    symbol = argv[3];
  }

  auto channel = grpc::CreateChannel(endpoint, grpc::InsecureChannelCredentials());
  auto stub = hermeneutic::grpc::AggregatorService::NewStub(channel);

  grpc::ClientContext context;
  hermeneutic::services::grpc_helpers::AttachAuth(context, token);
  hermeneutic::grpc::SubscribeRequest request;
  request.set_symbol(symbol);

  auto reader = stub->StreamBooks(&context, request);
  hermeneutic::grpc::AggregatedBook message;
  auto calculator = hermeneutic::price_bands::PriceBandsCalculator(
      hermeneutic::price_bands::defaultOffsets());
  spdlog::info("Price bands client connected to {}", endpoint);

  while (reader->Read(&message)) {
    auto view = hermeneutic::services::grpc_helpers::ToDomain(message);
    auto quotes = calculator.compute(view);
    for (const auto& quote : quotes) {
      spdlog::info("Offset {} bps -> bid {} ask {}",
                   quote.offset_bps.toString(0),
                   quote.bid_price.toString(2),
                   quote.ask_price.toString(2));
    }
  }

  auto status = reader->Finish();
  if (!status.ok()) {
    spdlog::error("Price bands stream closed: {}", status.error_message());
    return 1;
  }
  return 0;
}
