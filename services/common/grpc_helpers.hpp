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
  auto to_time_point = [](auto duration) {
    return std::chrono::system_clock::time_point(
        std::chrono::duration_cast<std::chrono::system_clock::duration>(duration));
  };
  if (message.publish_timestamp_ns() > 0) {
    view.timestamp = to_time_point(std::chrono::nanoseconds(message.publish_timestamp_ns()));
  } else {
    view.timestamp = to_time_point(std::chrono::milliseconds(message.timestamp_unix_millis()));
  }
  view.bid_levels.reserve(message.bid_levels_size());
  for (const auto& level : message.bid_levels()) {
    view.bid_levels.push_back({hermeneutic::common::Decimal::fromString(level.price()),
                               hermeneutic::common::Decimal::fromString(level.quantity())});
  }
  view.ask_levels.reserve(message.ask_levels_size());
  for (const auto& level : message.ask_levels()) {
    view.ask_levels.push_back({hermeneutic::common::Decimal::fromString(level.price()),
                               hermeneutic::common::Decimal::fromString(level.quantity())});
  }
  view.last_feed_timestamp_ns = message.last_feed_timestamp_ns();
  view.last_local_timestamp_ns = message.last_local_timestamp_ns();
  view.min_feed_timestamp_ns = message.min_feed_timestamp_ns();
  view.max_feed_timestamp_ns = message.max_feed_timestamp_ns();
  view.min_local_timestamp_ns = message.min_local_timestamp_ns();
  view.max_local_timestamp_ns = message.max_local_timestamp_ns();
  return view;
}

inline hermeneutic::grpc::AggregatedBook FromDomain(
    const hermeneutic::common::AggregatedBookView& view) {
  hermeneutic::grpc::AggregatedBook message;
  auto* bid = message.mutable_best_bid();
  bid->set_price(view.best_bid.price.toString(8));
  bid->set_quantity(view.best_bid.quantity.toString(8));
  auto* ask = message.mutable_best_ask();
  ask->set_price(view.best_ask.price.toString(8));
  ask->set_quantity(view.best_ask.quantity.toString(8));
  message.set_exchange_count(static_cast<std::uint32_t>(view.exchange_count));
  auto duration = view.timestamp.time_since_epoch();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
  message.set_timestamp_unix_millis(ms.count());
  auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
  message.set_publish_timestamp_ns(ns.count());
  for (const auto& level : view.bid_levels) {
    auto* proto_level = message.add_bid_levels();
    proto_level->set_price(level.price.toString(8));
    proto_level->set_quantity(level.quantity.toString(8));
  }
  for (const auto& level : view.ask_levels) {
    auto* proto_level = message.add_ask_levels();
    proto_level->set_price(level.price.toString(8));
    proto_level->set_quantity(level.quantity.toString(8));
  }
  message.set_last_feed_timestamp_ns(view.last_feed_timestamp_ns);
  message.set_last_local_timestamp_ns(view.last_local_timestamp_ns);
  message.set_min_feed_timestamp_ns(view.min_feed_timestamp_ns);
  message.set_max_feed_timestamp_ns(view.max_feed_timestamp_ns);
  message.set_min_local_timestamp_ns(view.min_local_timestamp_ns);
  message.set_max_local_timestamp_ns(view.max_local_timestamp_ns);
  return message;
}

inline void AttachAuth(::grpc::ClientContext& context, const std::string& token) {
  if (!token.empty()) {
    context.AddMetadata("authorization", "Bearer " + token);
  }
}

}  // namespace hermeneutic::services::grpc_helpers
