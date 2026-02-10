#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "tests/include/doctest_config.hpp"

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
  view.bid_levels = {
      {Decimal::fromString("100.00"), Decimal::fromString("2.0")},
      {Decimal::fromString("99.75"), Decimal::fromString("4.0")},
  };
  view.ask_levels = {
      {Decimal::fromString("100.50"), Decimal::fromString("0.1")},
      {Decimal::fromString("101.25"), Decimal::fromString("3.0")},
  };
  hermeneutic::volume_bands::VolumeBandsCalculator calculator(
      {Decimal::fromInteger(100), Decimal::fromInteger(500), Decimal::fromInteger(1'000)});
  auto quotes = calculator.compute(view);
  REQUIRE(quotes.size() == 3);
  CHECK(quotes[0].bid_price == Decimal::fromString("100.00"));
  CHECK(quotes[1].bid_price == Decimal::fromString("99.75"));
  CHECK(quotes[2].bid_price == Decimal::fromRaw(0));
  CHECK(quotes[0].ask_price == Decimal::fromString("101.25"));
  CHECK(quotes[1].ask_price == Decimal::fromRaw(0));
  CHECK(quotes[2].ask_price == Decimal::fromRaw(0));

  auto formatted = hermeneutic::volume_bands::formatQuote(quotes[0]);
  CHECK(formatted == "Bands 100 -> bid 100.00 ask 101.25");
}
