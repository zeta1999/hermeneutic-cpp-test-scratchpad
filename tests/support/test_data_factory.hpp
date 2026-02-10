#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include "hermeneutic/common/events.hpp"

namespace hermeneutic::tests::support {

inline std::chrono::system_clock::time_point timeFromNanoseconds(std::int64_t ns) {
  return std::chrono::system_clock::time_point(
      std::chrono::duration_cast<std::chrono::system_clock::duration>(
          std::chrono::nanoseconds(ns)));
}

inline common::BookEvent makeNewOrder(std::string exchange,
                                      std::uint64_t order_id,
                                      common::Side side,
                                      std::string price,
                                      std::string quantity,
                                      std::uint64_t sequence,
                                      std::chrono::system_clock::time_point timestamp = {}) {
  common::BookEvent event;
  event.exchange = std::move(exchange);
  event.kind = common::BookEventKind::NewOrder;
  event.sequence = sequence;
  event.order.order_id = order_id;
  event.order.side = side;
  event.order.price = common::Decimal::fromString(price);
  event.order.quantity = common::Decimal::fromString(quantity);
  event.timestamp = timestamp;
  return event;
}

}  // namespace hermeneutic::tests::support

