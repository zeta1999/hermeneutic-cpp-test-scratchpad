#include "hermeneutic/lob/order_book.hpp"

#include <stdexcept>

namespace hermeneutic::lob {

using common::BookEvent;
using common::BookEventKind;
using common::Decimal;
using common::MarketOrder;
using common::PriceLevel;

namespace {
const Decimal kZero = Decimal::fromRaw(0);

void applyDelta(common::Side side,
                common::Decimal price,
                common::Decimal delta,
                std::map<common::Decimal, common::Decimal, std::greater<common::Decimal>>& bids,
                std::map<common::Decimal, common::Decimal, std::less<common::Decimal>>& asks) {
  HERMENEUTIC_ASSERT_DEBUG(price >= kZero, "price must be non-negative");
  if (side == common::Side::Bid) {
    auto it = bids.find(price);
    Decimal next = delta;
    if (it != bids.end()) {
      next = it->second + delta;
    }
    if (next <= kZero) {
      if (it != bids.end()) {
        bids.erase(it);
      }
      return;
    }
    bids[price] = next;
    return;
  }

  auto it = asks.find(price);
  Decimal next = delta;
  if (it != asks.end()) {
    next = it->second + delta;
  }
  if (next <= kZero) {
    if (it != asks.end()) {
      asks.erase(it);
    }
    return;
  }
  asks[price] = next;
}

}  // namespace

void LimitOrderBook::validateInvariants() const {
#if defined(HERMENEUTIC_ENABLE_DEBUG_ASSERTS) && HERMENEUTIC_ENABLE_DEBUG_ASSERTS
  const Decimal kZero = Decimal::fromRaw(0);
  bool first = true;
  Decimal previous_price{};
  for (const auto& [price, qty] : bids_) {
    HERMENEUTIC_ASSERT_DEBUG(price >= kZero, "bid price negative");
    HERMENEUTIC_ASSERT_DEBUG(qty > kZero, "bid quantity non-positive");
    if (!first) {
      HERMENEUTIC_ASSERT_DEBUG(price < previous_price, "bid levels not strictly descending");
    }
    previous_price = price;
    first = false;
  }

  first = true;
  previous_price = Decimal{};
  for (const auto& [price, qty] : asks_) {
    HERMENEUTIC_ASSERT_DEBUG(price >= kZero, "ask price negative");
    HERMENEUTIC_ASSERT_DEBUG(qty > kZero, "ask quantity non-positive");
    if (!first) {
      HERMENEUTIC_ASSERT_DEBUG(price > previous_price, "ask levels not strictly ascending");
    }
    previous_price = price;
    first = false;
  }

  if (!bids_.empty() && !asks_.empty()) {
    HERMENEUTIC_ASSERT_DEBUG(asks_.begin()->first > bids_.begin()->first,
                             "best ask must exceed best bid");
  }
#endif
}

void LimitOrderBook::apply(const BookEvent& event) {
  if (!exchange_name_.empty() && exchange_name_ != event.exchange) {
    throw std::invalid_argument("update exchange mismatch");
  }
  if (exchange_name_.empty()) {
    exchange_name_ = event.exchange;
  }

  auto now = std::chrono::system_clock::now();
  auto toNs = [](std::chrono::system_clock::time_point tp) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(tp.time_since_epoch()).count();
  };
  std::int64_t feed_ns = event.feed_timestamp_ns;
  if (feed_ns == 0) {
    auto feed_tp = event.timestamp.time_since_epoch().count() == 0 ? now : event.timestamp;
    feed_ns = toNs(feed_tp);
  }
  std::int64_t local_ns = event.local_timestamp_ns;
  if (local_ns == 0) {
    local_ns = toNs(now);
  }
  last_feed_timestamp_ns_ = feed_ns;
  last_local_timestamp_ns_ = local_ns;

  if (event.sequence != 0 && event.sequence <= last_sequence_) {
    return;
  }
  if (event.sequence != 0) {
    last_sequence_ = event.sequence;
  }

  switch (event.kind) {
    case BookEventKind::Snapshot: {
      bids_.clear();
      asks_.clear();
      orders_.clear();
      for (const auto& level : event.snapshot.bids) {
        if (level.quantity > kZero) {
          bids_[level.price] = level.quantity;
        }
      }
      for (const auto& level : event.snapshot.asks) {
        if (level.quantity > kZero) {
          asks_[level.price] = level.quantity;
        }
      }
      break;
    }
    case BookEventKind::NewOrder: {
      const auto& order = event.order;
      if (order.order_id == 0) {
        break;
      }
      auto existing = orders_.find(order.order_id);
      if (existing != orders_.end()) {
        auto removal = common::Decimal::fromRaw(0) - existing->second.quantity;
        applyDelta(existing->second.side, existing->second.price, removal, bids_, asks_);
        orders_.erase(existing);
      }
      orders_.emplace(order.order_id, order);
      applyDelta(order.side, order.price, order.quantity, bids_, asks_);
      break;
    }
    case BookEventKind::CancelOrder: {
      if (event.order.order_id == 0) {
        break;
      }
      auto existing = orders_.find(event.order.order_id);
      if (existing == orders_.end()) {
        break;
      }
      auto removal = common::Decimal::fromRaw(0) - existing->second.quantity;
      applyDelta(existing->second.side, existing->second.price, removal, bids_, asks_);
      orders_.erase(existing);
      break;
    }
  }
  validateInvariants();
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

LimitOrderBook::BidLevelIterator LimitOrderBook::bidLevelsBegin() const {
  return bids_.cbegin();
}

LimitOrderBook::BidLevelIterator LimitOrderBook::bidLevelsEnd() const {
  return bids_.cend();
}

LimitOrderBook::AskLevelIterator LimitOrderBook::askLevelsBegin() const {
  return asks_.cbegin();
}

LimitOrderBook::AskLevelIterator LimitOrderBook::askLevelsEnd() const {
  return asks_.cend();
}

LimitOrderBook::OrderIterator LimitOrderBook::limitOrdersBegin() const {
  return orders_.cbegin();
}

LimitOrderBook::OrderIterator LimitOrderBook::limitOrdersEnd() const {
  return orders_.cend();
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
