#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "hermeneutic/common/decimal.hpp"
#include "hermeneutic/common/enum.hpp"

// TODO style: too much code/lines?
namespace hermeneutic::common {

HERMENEUTIC_ENUM(Side, Bid, Ask);

inline std::string_view toString(Side side) {
  return SideToString(side);
}

inline Side parseSide(const std::string& text) { return StringToSide(text); }

inline Side parseSide(std::string_view text) {
  return parseSide(std::string(text));
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

HERMENEUTIC_ENUM(BookEventKind, NewOrder, CancelOrder, Snapshot);

inline std::string_view toString(BookEventKind kind) {
  return BookEventKindToString(kind);
}

inline BookEventKind parseBookEventKind(const std::string& text) {
  return StringToBookEventKind(text);
}

inline BookEventKind parseBookEventKind(std::string_view text) {
  return parseBookEventKind(std::string(text));
}

struct MarketOrder {
  std::uint64_t order_id{0};
  Side side{Side::Bid};
  Decimal price{};
  Decimal quantity{};
};

// TODO: style ... 
struct BookEvent {
  std::string exchange;
  BookEventKind kind{BookEventKind::NewOrder};
  std::uint64_t sequence{0};
  MarketOrder order;
  OrderBookSnapshot snapshot;
  std::chrono::system_clock::time_point timestamp{};
  std::int64_t feed_timestamp_ns{0};
  std::int64_t local_timestamp_ns{0};
};

struct AggregatedQuote {
  Decimal price;
  Decimal quantity;
};

struct AggregatedBookView {
  std::vector<PriceLevel> bid_levels;
  std::vector<PriceLevel> ask_levels;
  AggregatedQuote best_bid;
  AggregatedQuote best_ask;
  std::chrono::system_clock::time_point timestamp{};
  std::size_t exchange_count{};
  // Feed timestamps (nanoseconds since epoch, UTC) aggregated across exchanges.
  std::int64_t last_feed_timestamp_ns{0};
  std::int64_t last_local_timestamp_ns{0};
  std::int64_t min_feed_timestamp_ns{0};
  std::int64_t max_feed_timestamp_ns{0};
  std::int64_t min_local_timestamp_ns{0};
  std::int64_t max_local_timestamp_ns{0};
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
