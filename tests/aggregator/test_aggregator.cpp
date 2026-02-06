#include <chrono>
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <thread>

#include "hermeneutic/aggregator/aggregator.hpp"
#include "hermeneutic/common/events.hpp"

using hermeneutic::common::Decimal;
using hermeneutic::common::MarketUpdate;
using hermeneutic::common::Side;

TEST_CASE("aggregator consolidates best bid and ask") {
  hermeneutic::aggregator::AggregationEngine engine;
  engine.start();
  MarketUpdate binance{.exchange = "binance",
                       .side = Side::Bid,
                       .price = Decimal::fromString("100.00"),
                       .quantity = Decimal::fromInteger(1)};
  MarketUpdate coinbase{.exchange = "coinbase",
                        .side = Side::Bid,
                        .price = Decimal::fromString("101.00"),
                        .quantity = Decimal::fromInteger(2)};
  engine.push(binance);
  engine.push(coinbase);
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  auto view = engine.latest();
  CHECK(view.best_bid.price.toString(2) == "101.00");
  CHECK(view.best_bid.quantity.toString(0) == "2");
  engine.stop();
}
