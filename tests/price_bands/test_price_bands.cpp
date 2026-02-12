#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <sstream>

#include "tests/include/doctest_config.hpp"

#include "hermeneutic/common/events.hpp"
#include "hermeneutic/price_bands/price_bands_publisher.hpp"

using hermeneutic::common::Decimal;
using hermeneutic::common::DecimalWide;

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
  view.best_bid.quantity = Decimal::fromString("1");
  view.best_ask.quantity = Decimal::fromString("1");
  hermeneutic::price_bands::PriceBandsCalculator calculator(
      {Decimal::fromInteger(50), Decimal::fromInteger(500)});
  auto quotes = calculator.compute(view);
  REQUIRE(quotes.size() == 2);

  auto expect_decimal = [](const Decimal& price, const Decimal& bps, bool bid) {
    const Decimal fraction = bps / Decimal::fromInteger(10'000);
    const auto widen = [](const Decimal& value) {
      return DecimalWide::fromRaw(value.raw());
    };
    const DecimalWide wide_one = DecimalWide::fromInteger(1);
    const DecimalWide wide_price = widen(price);
    const DecimalWide wide_fraction = widen(fraction);
    const DecimalWide factor = bid ? (wide_one - wide_fraction) : (wide_one + wide_fraction);
    const DecimalWide adjusted = wide_price * factor;
    return Decimal::fromRaw(adjusted.raw());
  };

  const auto offsets = std::array{Decimal::fromInteger(50), Decimal::fromInteger(500)};
  for (std::size_t i = 0; i < offsets.size(); ++i) {
    CAPTURE(i);
    const auto offset = offsets[i];
    const auto expected_bid = expect_decimal(view.best_bid.price, offset, true);
    const auto expected_ask = expect_decimal(view.best_ask.price, offset, false);
    const double bid_delta =
        quotes[i].bid_price.toDouble() - expected_bid.toDouble();
    std::ostringstream bid_info;
    bid_info << "offset=" << offset.toString(0)
             << " bid_actual=" << quotes[i].bid_price.toString(10)
             << " bid_expected=" << expected_bid.toString(10)
             << " bid_delta_double=" << bid_delta;
    DOCTEST_INFO(bid_info.str());

    const double ask_delta =
        quotes[i].ask_price.toDouble() - expected_ask.toDouble();
    std::ostringstream ask_info;
    ask_info << "offset=" << offset.toString(0)
             << " ask_actual=" << quotes[i].ask_price.toString(10)
             << " ask_expected=" << expected_ask.toString(10)
             << " ask_delta_double=" << ask_delta;
    DOCTEST_INFO(ask_info.str());
    CHECK(quotes[i].bid_price == expected_bid);
    CHECK(quotes[i].ask_price == expected_ask);
  }

  CHECK(quotes[0].bid_price > Decimal::fromInteger(0));
  CHECK(quotes[0].ask_price > Decimal::fromInteger(0));
}

TEST_CASE("Price bands require both best bid and ask") {
  hermeneutic::common::AggregatedBookView view{};
  view.best_bid.price = Decimal::fromString("100.00");
  view.best_bid.quantity = Decimal::fromString("1");
  view.best_ask.price = Decimal::fromString("101.00");
  view.best_ask.quantity = Decimal::fromRaw(0);

  hermeneutic::price_bands::PriceBandsCalculator calculator({Decimal::fromInteger(50)});
  auto quotes = calculator.compute(view);
  CHECK(quotes.empty());
}
