#include "hermeneutic/lob/order_book.hpp"

#include <stdexcept>

namespace hermeneutic::lob {

using common::Decimal;
using common::MarketUpdate;
using common::PriceLevel;

namespace {
const Decimal kZero = Decimal::fromRaw(0);
}

void LimitOrderBook::apply(const MarketUpdate& update) {
  if (!exchange_name_.empty() && exchange_name_ != update.exchange) {
    throw std::invalid_argument("update exchange mismatch");
  }
  if (exchange_name_.empty()) {
    exchange_name_ = update.exchange;
  }

  if (update.side == common::Side::Bid) {
    auto iterator = bids_.find(update.price);
    if (update.quantity <= kZero) {
      if (iterator != bids_.end()) {
        bids_.erase(iterator);
      }
      return;
    }
    bids_[update.price] = update.quantity;
    return;
  }

  auto iterator = asks_.find(update.price);
  if (update.quantity <= kZero) {
    if (iterator != asks_.end()) {
      asks_.erase(iterator);
    }
    return;
  }
  asks_[update.price] = update.quantity;
}

common::PriceLevel LimitOrderBook::bestBid() const {
  if (bids_.empty()) {
    return PriceLevel{Decimal::fromRaw(0), Decimal::fromRaw(0)};
  }
  auto& level = *bids_.begin();
  return PriceLevel{level.first, level.second};
}

common::PriceLevel LimitOrderBook::bestAsk() const {
  if (asks_.empty()) {
    return PriceLevel{Decimal::fromRaw(0), Decimal::fromRaw(0)};
  }
  auto& level = *asks_.begin();
  return PriceLevel{level.first, level.second};
}

common::OrderBookSnapshot LimitOrderBook::snapshot(std::size_t depth) const {
  common::OrderBookSnapshot snap;
  snap.bids.reserve(depth);
  snap.asks.reserve(depth);

  std::size_t count = 0;
  for (const auto& [price, qty] : bids_) {
    if (count++ >= depth) {
      break;
    }
    snap.bids.push_back({price, qty});
  }

  count = 0;
  for (const auto& [price, qty] : asks_) {
    if (count++ >= depth) {
      break;
    }
    snap.asks.push_back({price, qty});
  }
  return snap;
}

bool LimitOrderBook::empty() const {
  return bids_.empty() && asks_.empty();
}

}  // namespace hermeneutic::lob
