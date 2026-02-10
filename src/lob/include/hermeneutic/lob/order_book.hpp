#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "hermeneutic/common/assert.hpp"
#include "hermeneutic/common/events.hpp"

namespace hermeneutic::lob {

class LimitOrderBook {
 public:
  using BidMap = std::map<common::Decimal, common::Decimal, std::greater<common::Decimal>>;
  using AskMap = std::map<common::Decimal, common::Decimal, std::less<common::Decimal>>;
  using OrderMap = std::unordered_map<std::uint64_t, common::MarketOrder>;

  void apply(const common::BookEvent& event);

  common::PriceLevel bestBid() const;
  common::PriceLevel bestAsk() const;

  // Level iterators keep callers at the aggregated price/quantity view.
  using BidLevelIterator = BidMap::const_iterator;
  using AskLevelIterator = AskMap::const_iterator;
  BidLevelIterator bidLevelsBegin() const;
  BidLevelIterator bidLevelsEnd() const;
  AskLevelIterator askLevelsBegin() const;
  AskLevelIterator askLevelsEnd() const;

  // Expose limit-order iteration for callers that need per-order details.
  using OrderIterator = OrderMap::const_iterator;
  OrderIterator limitOrdersBegin() const;
  OrderIterator limitOrdersEnd() const;

  std::int64_t lastFeedTimestampNs() const { return last_feed_timestamp_ns_; }
  std::int64_t lastLocalUpdateTimestampNs() const { return last_local_timestamp_ns_; }

  common::OrderBookSnapshot snapshot(std::size_t depth) const;
  bool empty() const;
  const std::string& exchange() const { return exchange_name_; }
  void setExchange(std::string name) { exchange_name_ = std::move(name); }

 private:
  void validateInvariants() const;

  BidMap bids_;
  AskMap asks_;
  OrderMap orders_;
  std::uint64_t last_sequence_{0};
  std::string exchange_name_;
  std::int64_t last_feed_timestamp_ns_{0};
  std::int64_t last_local_timestamp_ns_{0};
};

}  // namespace hermeneutic::lob
