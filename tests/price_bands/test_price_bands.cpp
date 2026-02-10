#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#ifndef REQUIRE
#define REQUIRE(...) CHECK(__VA_ARGS__)
#endif

#include "hermeneutic/common/events.hpp"
#include "hermeneutic/price_bands/price_bands_publisher.hpp"

using hermeneutic::common::Decimal;

TEST_CASE("Price bands calculator applies offsets") {
  auto view = hermeneutic::common::AggregatedBookView{};
  view.best_bid.price = Decimal::fromString("100.00");
  view.best_ask.price = Decimal::fromString("101.00");
  view.best_bid.quantity = Decimal::fromString("1");
  view.best_ask.quantity = Decimal::fromString("1");

  hermeneutic::price_bands::PriceBandsCalculator calculator(
      {Decimal::fromInteger(50)});
  auto quotes = calculator.compute(view);
  REQUIRE(quotes.size() == 1);

  Decimal offset = Decimal::fromInteger(50);
  Decimal fraction = offset / Decimal::fromInteger(10'000);
  Decimal expected_bid = view.best_bid.price * (Decimal::fromInteger(1) - fraction);
  Decimal expected_ask = view.best_ask.price * (Decimal::fromInteger(1) + fraction);

  CHECK(quotes[0].bid_price == expected_bid);
  CHECK(quotes[0].ask_price == expected_ask);

  auto line = hermeneutic::price_bands::formatQuote(quotes[0]);
  CHECK(line == "Offset 50 bps -> bid " + expected_bid.toString(2) + " ask " + expected_ask.toString(2));
}
