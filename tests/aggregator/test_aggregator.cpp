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

TEST_CASE("aggregator exposes aggregated depth levels") {
  hermeneutic::aggregator::AggregationEngine engine;
  engine.start();
  engine.push(makeNewOrder("ex1", 1, Side::Bid, "100.00", "1", 1));
  engine.push(makeNewOrder("ex1", 2, Side::Bid, "101.00", "2", 2));
  engine.push(makeNewOrder("ex2", 3, Side::Bid, "100.00", "3", 3));
  engine.push(makeNewOrder("ex2", 4, Side::Ask, "105.00", "4", 4));
  engine.push(makeNewOrder("ex3", 5, Side::Ask, "106.00", "5", 5));
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  auto view = engine.latest();
  CHECK(view.bid_levels.size() >= 2);
  CHECK(view.bid_levels[0].price.toString(2) == "101.00");
  CHECK(view.bid_levels[1].price.toString(2) == "100.00");
  CHECK(view.bid_levels[1].quantity.toString(0) == "4");
  CHECK(view.ask_levels.size() >= 2);
  CHECK(view.ask_levels[0].price.toString(2) == "105.00");
  CHECK(view.ask_levels[1].price.toString(2) == "106.00");
  engine.stop();
}
