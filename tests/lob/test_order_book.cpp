#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "hermeneutic/common/events.hpp"
#include "hermeneutic/lob/order_book.hpp"

using hermeneutic::common::Decimal;
using hermeneutic::common::MarketUpdate;
using hermeneutic::common::Side;

TEST_CASE("limit order book tracks bids and asks") {
  hermeneutic::lob::LimitOrderBook book;
  MarketUpdate bid{.exchange = "cex-1",
                   .side = Side::Bid,
                   .price = Decimal::fromString("100.00"),
                   .quantity = Decimal::fromInteger(2)};
  book.apply(bid);
  auto best_bid = book.bestBid();
  CHECK(best_bid.price.toString(2) == "100.00");
  CHECK(best_bid.quantity.toString(0) == "2");

  MarketUpdate ask{.exchange = "cex-1",
                   .side = Side::Ask,
                   .price = Decimal::fromString("101.00"),
                   .quantity = Decimal::fromInteger(1)};
  book.apply(ask);
  auto best_ask = book.bestAsk();
  CHECK(best_ask.price.toString(2) == "101.00");
  CHECK(best_ask.quantity.toString(0) == "1");
}

TEST_CASE("limit order book removes zero quantity levels") {
  hermeneutic::lob::LimitOrderBook book;
  MarketUpdate bid{.exchange = "cex-1",
                   .side = Side::Bid,
                   .price = Decimal::fromString("100.00"),
                   .quantity = Decimal::fromInteger(2)};
  book.apply(bid);
  bid.quantity = Decimal::fromRaw(0);
  book.apply(bid);
  CHECK(book.bestBid().quantity == Decimal::fromRaw(0));
}
