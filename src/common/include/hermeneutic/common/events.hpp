#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
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

struct OrderBookSnapshot {
  std::vector<PriceLevel> bids;
  std::vector<PriceLevel> asks;
};

enum class BookEventKind { NewOrder, CancelOrder, Snapshot };

struct MarketOrder {
  std::string order_id;
  Side side{Side::Bid};
  Decimal price{};
  Decimal quantity{};
};

struct BookEvent {
  std::string exchange;
  BookEventKind kind{BookEventKind::NewOrder};
  std::uint64_t sequence{0};
  MarketOrder order;
  OrderBookSnapshot snapshot;
  std::chrono::system_clock::time_point timestamp{};
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
