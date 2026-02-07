#include <chrono>
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cstdint>
#include <thread>

#include "hermeneutic/aggregator/aggregator.hpp"
#include "hermeneutic/common/events.hpp"

using hermeneutic::common::BookEvent;
using hermeneutic::common::BookEventKind;
using hermeneutic::common::Decimal;
using hermeneutic::common::Side;

namespace {

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

}  // namespace

TEST_CASE("aggregator consolidates best bid and ask") {
  hermeneutic::aggregator::AggregationEngine engine;
  engine.start();
  engine.push(makeNewOrder("notbinance", 1, Side::Bid, "100.00", "1", 1));
  engine.push(makeNewOrder("notcoinbase", 2, Side::Bid, "101.00", "2", 2));
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  auto view = engine.latest();
  CHECK(view.best_bid.price.toString(2) == "101.00");
  CHECK(view.best_bid.quantity.toString(0) == "2");
  engine.stop();
}
