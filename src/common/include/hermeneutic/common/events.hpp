#pragma once

#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "hermeneutic/common/decimal.hpp"

namespace hermeneutic::common {

enum class Side { Bid, Ask };

inline std::string_view toString(Side side) {
  return side == Side::Bid ? "bid" : "ask";
}

inline Side invert(Side side) {
  return side == Side::Bid ? Side::Ask : Side::Bid;
}

struct PriceLevel {
  Decimal price;
  Decimal quantity;
};

struct MarketUpdate {
  std::string exchange;
  Side side{Side::Bid};
  Decimal price{};
  Decimal quantity{};
  std::chrono::system_clock::time_point timestamp{};
};

struct OrderBookSnapshot {
  std::vector<PriceLevel> bids;
  std::vector<PriceLevel> asks;
};

struct AggregatedQuote {
  Decimal price;
  Decimal quantity;
};

struct AggregatedBookView {
  AggregatedQuote best_bid;
  AggregatedQuote best_ask;
  std::chrono::system_clock::time_point timestamp{};
  std::size_t exchange_count{};
};

struct VolumeBandQuote {
  Decimal notional;
  Decimal bid_price;
  Decimal ask_price;
};

struct PriceBandQuote {
  Decimal offset_bps;
  Decimal bid_price;
  Decimal ask_price;
};

}  // namespace hermeneutic::common
