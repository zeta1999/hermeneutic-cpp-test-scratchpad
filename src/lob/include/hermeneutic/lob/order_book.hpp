#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "hermeneutic/common/events.hpp"

namespace hermeneutic::lob {

class LimitOrderBook {
 public:
  void apply(const common::BookEvent& event);

  common::PriceLevel bestBid() const;
  common::PriceLevel bestAsk() const;

  common::OrderBookSnapshot snapshot(std::size_t depth) const;
  bool empty() const;
  const std::string& exchange() const { return exchange_name_; }
  void setExchange(std::string name) { exchange_name_ = std::move(name); }

 private:
  using BidMap = std::map<common::Decimal, common::Decimal, std::greater<common::Decimal>>;
  using AskMap = std::map<common::Decimal, common::Decimal, std::less<common::Decimal>>;
  BidMap bids_;
  AskMap asks_;
  std::unordered_map<std::string, common::MarketOrder> orders_;
  std::uint64_t last_sequence_{0};
  std::string exchange_name_;
};

}  // namespace hermeneutic::lob
