#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "tests/include/doctest_config.hpp"

#include "hermeneutic/bbo/bbo_publisher.hpp"
#include "hermeneutic/common/events.hpp"
#include "hermeneutic/price_bands/price_bands_publisher.hpp"
#include "hermeneutic/volume_bands/volume_bands_publisher.hpp"

using hermeneutic::common::AggregatedBookView;
using hermeneutic::common::Decimal;

namespace {
Decimal adjustPriceForTest(const Decimal& price, const Decimal& offset_bps, bool bid) {
  const auto one = hermeneutic::common::DecimalWide::fromInteger(1);
  const auto scale = hermeneutic::common::DecimalWide::fromInteger(10'000);
  const auto wide_price = hermeneutic::common::DecimalWide::fromRaw(price.raw());
  const auto wide_offset = hermeneutic::common::DecimalWide::fromRaw(offset_bps.raw());
  const auto fraction = wide_offset / scale;
  const auto factor = bid ? (one - fraction) : (one + fraction);
  return Decimal::fromRaw((wide_price * factor).raw());
}
}  // namespace

TEST_CASE("bbo publisher keeps 8 decimal precision") {
  AggregatedBookView view;
  view.best_bid.price = Decimal::fromString("12345.67890123");
  view.best_bid.quantity = Decimal::fromString("0.12345678");
  view.best_ask.price = Decimal::fromString("12346.00000000");
  view.best_ask.quantity = Decimal::fromString("0.50000000");
  view.exchange_count = 2;

  hermeneutic::bbo::BboPublisher publisher;
  auto line = publisher.format(view);
  CHECK(line == "BEST_BID=12345.67890123@0.12345678 BEST_ASK=12346.00000000@0.50000000 EXCHANGES=2");
}

TEST_CASE("volume bands calculator gates by notional") {
  AggregatedBookView view;
  view.best_bid.price = Decimal::fromString("100.00");
  view.best_bid.quantity = Decimal::fromString("2.0");
  view.best_ask.price = Decimal::fromString("100.50");
  view.best_ask.quantity = Decimal::fromString("0.2");

  hermeneutic::volume_bands::VolumeBandsCalculator calculator(
      {Decimal::fromInteger(100), Decimal::fromInteger(500)});
  auto quotes = calculator.compute(view);
  REQUIRE(quotes.size() == 2);
  CHECK(quotes[0].bid_price == view.best_bid.price);
  CHECK(quotes[0].ask_price == Decimal::fromRaw(0));
  CHECK(quotes[1].bid_price == Decimal::fromRaw(0));
  auto line = hermeneutic::volume_bands::formatQuote(quotes[0]);
  CHECK(line == "Bands 100 -> bid 100.00 ask 0.00");
}

TEST_CASE("price bands calculator applies offsets") {
  AggregatedBookView view;
  view.best_bid.price = Decimal::fromString("200.00");
  view.best_ask.price = Decimal::fromString("201.00");
  hermeneutic::price_bands::PriceBandsCalculator calculator({Decimal::fromInteger(50)});
  auto quotes = calculator.compute(view);
  REQUIRE(quotes.size() == 1);
  Decimal offset = Decimal::fromInteger(50);
  auto expected_bid = adjustPriceForTest(view.best_bid.price, offset, true);
  auto expected_ask = adjustPriceForTest(view.best_ask.price, offset, false);
  CHECK(quotes[0].bid_price == expected_bid);
  CHECK(quotes[0].ask_price == expected_ask);
  auto line = hermeneutic::price_bands::formatQuote(quotes[0]);
  CHECK(line == "Offset 50 bps -> bid " + expected_bid.toString(2) + " ask " + expected_ask.toString(2));
}
