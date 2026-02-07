#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cstdint>

#include "hermeneutic/common/events.hpp"
#include "hermeneutic/lob/order_book.hpp"

using hermeneutic::common::BookEvent;
using hermeneutic::common::BookEventKind;
using hermeneutic::common::Decimal;
using hermeneutic::common::Side;

namespace {

BookEvent makeNewOrder(std::string id,
                       hermeneutic::common::Side side,
                       std::string price,
                       std::string quantity,
                       std::uint64_t sequence) {
  BookEvent event;
  event.exchange = "cex-1";
  event.kind = BookEventKind::NewOrder;
  event.sequence = sequence;
  event.order.order_id = std::move(id);
  event.order.side = side;
  event.order.price = Decimal::fromString(price);
  event.order.quantity = Decimal::fromString(quantity);
  return event;
}

BookEvent makeCancel(std::string id, std::uint64_t sequence) {
  BookEvent event;
  event.exchange = "cex-1";
  event.kind = BookEventKind::CancelOrder;
  event.sequence = sequence;
  event.order.order_id = std::move(id);
  return event;
}

}  // namespace

TEST_CASE("limit order book tracks bids and asks") {
  hermeneutic::lob::LimitOrderBook book;
  book.apply(makeNewOrder("order-1", Side::Bid, "100.00", "2", 1));
  auto best_bid = book.bestBid();
  CHECK(best_bid.price.toString(2) == "100.00");
  CHECK(best_bid.quantity.toString(0) == "2");

  book.apply(makeNewOrder("order-2", Side::Ask, "101.00", "1", 2));
  auto best_ask = book.bestAsk();
  CHECK(best_ask.price.toString(2) == "101.00");
  CHECK(best_ask.quantity.toString(0) == "1");
}

TEST_CASE("limit order book removes zero quantity levels") {
  hermeneutic::lob::LimitOrderBook book;
  book.apply(makeNewOrder("order-1", Side::Bid, "100.00", "2", 1));
  book.apply(makeCancel("order-1", 2));
  CHECK(book.bestBid().quantity == Decimal::fromRaw(0));
}
