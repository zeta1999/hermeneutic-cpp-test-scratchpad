#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#ifndef REQUIRE
#define REQUIRE(...) CHECK(__VA_ARGS__)
#endif

#include "hermeneutic/common/events.hpp"
#include "hermeneutic/volume_bands/volume_bands_publisher.hpp"

using hermeneutic::common::Decimal;

TEST_CASE("Volume bands calculator gates by notional") {
  auto makeView = [] {
    hermeneutic::common::AggregatedBookView view;
    view.best_bid.price = Decimal::fromString("100.00");
    view.best_bid.quantity = Decimal::fromString("2.0");  // 200 notional
    view.best_ask.price = Decimal::fromString("100.50");
    view.best_ask.quantity = Decimal::fromString("0.1");
    return view;
  };

  auto view = makeView();
  hermeneutic::volume_bands::VolumeBandsCalculator calculator(
      {Decimal::fromInteger(100), Decimal::fromInteger(500)});
  auto quotes = calculator.compute(view);
  REQUIRE(quotes.size() == 2);
  CHECK(quotes[0].bid_price == view.best_bid.price);
  CHECK(quotes[0].ask_price == Decimal::fromRaw(0));
  CHECK(quotes[1].bid_price == Decimal::fromRaw(0));
  CHECK(quotes[1].ask_price == Decimal::fromRaw(0));

  auto formatted = hermeneutic::volume_bands::formatQuote(quotes[0]);
  CHECK(formatted == "Bands 100 -> bid 100.00 ask 0.00");
}
