#pragma once

#include <grpcpp/grpcpp.h>

#include <chrono>
#include <string>

#include "aggregator.grpc.pb.h"
#include "hermeneutic/common/events.hpp"

namespace hermeneutic::services::grpc_helpers {

inline hermeneutic::common::AggregatedBookView ToDomain(
    const hermeneutic::grpc::AggregatedBook& message) {
  hermeneutic::common::AggregatedBookView view;
  view.best_bid.price = hermeneutic::common::Decimal::fromString(message.best_bid().price());
  view.best_bid.quantity = hermeneutic::common::Decimal::fromString(message.best_bid().quantity());
  view.best_ask.price = hermeneutic::common::Decimal::fromString(message.best_ask().price());
  view.best_ask.quantity = hermeneutic::common::Decimal::fromString(message.best_ask().quantity());
  view.exchange_count = message.exchange_count();
  view.timestamp = std::chrono::system_clock::time_point(std::chrono::milliseconds(
      message.timestamp_unix_millis()));
  return view;
}

inline void AttachAuth(::grpc::ClientContext& context, const std::string& token) {
  if (!token.empty()) {
    context.AddMetadata("authorization", "Bearer " + token);
  }
}

}  // namespace hermeneutic::services::grpc_helpers
