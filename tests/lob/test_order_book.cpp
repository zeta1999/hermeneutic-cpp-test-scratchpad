#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "hermeneutic/common/events.hpp"
#include "hermeneutic/lob/order_book.hpp"

using hermeneutic::common::BookEvent;
using hermeneutic::common::BookEventKind;
using hermeneutic::common::Decimal;
using hermeneutic::common::Side;

namespace {

BookEvent makeNewOrder(std::uint64_t id,
                       hermeneutic::common::Side side,
                       std::string price,
                       std::string quantity,
                       std::uint64_t sequence) {
  BookEvent event;
  event.exchange = "cex-1";
  event.kind = BookEventKind::NewOrder;
  event.sequence = sequence;
  event.order.order_id = id;
  event.order.side = side;
  event.order.price = Decimal::fromString(price);
  event.order.quantity = Decimal::fromString(quantity);
  return event;
}

BookEvent makeCancel(std::uint64_t id, std::uint64_t sequence) {
  BookEvent event;
  event.exchange = "cex-1";
  event.kind = BookEventKind::CancelOrder;
  event.sequence = sequence;
  event.order.order_id = id;
  return event;
}

}  // namespace

TEST_CASE("limit order book tracks bids and asks") {
  hermeneutic::lob::LimitOrderBook book;
  book.apply(makeNewOrder(1, Side::Bid, "100.00", "2", 1));
  auto best_bid = book.bestBid();
  CHECK(best_bid.price.toString(2) == "100.00");
  CHECK(best_bid.quantity.toString(0) == "2");

  book.apply(makeNewOrder(2, Side::Ask, "101.00", "1", 2));
  auto best_ask = book.bestAsk();
  CHECK(best_ask.price.toString(2) == "101.00");
  CHECK(best_ask.quantity.toString(0) == "1");
}

TEST_CASE("limit order book removes zero quantity levels") {
  hermeneutic::lob::LimitOrderBook book;
  book.apply(makeNewOrder(1, Side::Bid, "100.00", "2", 1));
  book.apply(makeCancel(1, 2));
  CHECK(book.bestBid().quantity == Decimal::fromRaw(0));
}

TEST_CASE("limit order book exposes level iterators without limit-order details") {
  hermeneutic::lob::LimitOrderBook book;
  book.apply(makeNewOrder(1, Side::Bid, "100.00", "2", 1));
  book.apply(makeNewOrder(2, Side::Bid, "101.00", "3", 2));
  book.apply(makeNewOrder(3, Side::Ask, "105.00", "1", 3));
  book.apply(makeNewOrder(4, Side::Ask, "106.00", "4", 4));

  std::vector<std::string> bid_prices;
  for (auto it = book.bidLevelsBegin(); it != book.bidLevelsEnd(); ++it) {
    bid_prices.push_back(it->first.toString(2));
  }
  CHECK(bid_prices == std::vector<std::string>({"101.00", "100.00"}));

  std::vector<std::string> ask_prices;
  for (auto it = book.askLevelsBegin(); it != book.askLevelsEnd(); ++it) {
    ask_prices.push_back(it->first.toString(2));
  }
  CHECK(ask_prices == std::vector<std::string>({"105.00", "106.00"}));
}

TEST_CASE("limit order book iterates limit orders when needed") {
  hermeneutic::lob::LimitOrderBook book;
  book.apply(makeNewOrder(1, Side::Bid, "100.00", "2", 1));
  book.apply(makeNewOrder(2, Side::Ask, "105.00", "5", 2));

  std::vector<std::uint64_t> ids;
  for (auto it = book.limitOrdersBegin(); it != book.limitOrdersEnd(); ++it) {
    ids.push_back(it->first);
  }
  std::sort(ids.begin(), ids.end());
  CHECK(ids == std::vector<std::uint64_t>({1, 2}));
}
