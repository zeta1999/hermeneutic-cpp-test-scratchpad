#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cmath>

#include "tests/include/doctest_config.hpp"

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

TEST_CASE("Price bands stay positive for large quotes") {
  auto view = hermeneutic::common::AggregatedBookView{};
  view.best_bid.price = Decimal::fromString("30045.49");
  view.best_ask.price = Decimal::fromString("30050.50");
  hermeneutic::price_bands::PriceBandsCalculator calculator(
      {Decimal::fromInteger(50), Decimal::fromInteger(500)});
  auto quotes = calculator.compute(view);
  REQUIRE(quotes.size() == 2);

  auto expect = [](double price, double bps, bool bid) {
    const double fraction = bps / 10'000.0;
    const double factor = bid ? (1.0 - fraction) : (1.0 + fraction);
    return price * factor;
  };

  auto close = [](double lhs, double rhs) {
    return std::fabs(lhs - rhs) < 1e-6;
  };

  CHECK(close(quotes[0].bid_price.toDouble(), expect(30045.49, 50, true)));
  CHECK(close(quotes[0].ask_price.toDouble(), expect(30050.50, 50, false)));
  CHECK(close(quotes[1].bid_price.toDouble(), expect(30045.49, 500, true)));
  CHECK(close(quotes[1].ask_price.toDouble(), expect(30050.50, 500, false)));
  CHECK(quotes[0].bid_price.toDouble() > 0.0);
  CHECK(quotes[0].ask_price.toDouble() > 0.0);
}
