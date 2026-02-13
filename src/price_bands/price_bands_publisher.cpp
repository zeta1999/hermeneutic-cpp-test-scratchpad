#include "hermeneutic/price_bands/price_bands_publisher.hpp"

#include <sstream>

#include "hermeneutic/common/assert.hpp"

namespace hermeneutic::price_bands {
namespace {
const common::Decimal kZero = common::Decimal::fromRaw(0);
const common::Decimal kOne = common::Decimal::fromInteger(1);
const common::Decimal kTenThousand = common::Decimal::fromInteger(10'000);

inline common::DecimalWide widen(const common::Decimal& value) {
  return common::DecimalWide::fromRaw(value.raw());
}

common::Decimal adjustPrice(const common::Decimal& price,
                            const common::Decimal& offset_bps,
                            bool bid) {
  // Compute the fraction in the wide decimal space to avoid overflowing the
  // 128-bit intermediate when offset_bps * scale exceeds int128 capacity.
  auto wide_price = widen(price);
  auto wide_fraction = widen(offset_bps) / widen(kTenThousand);
  auto wide_one = widen(kOne);
  common::DecimalWide factor = bid ? (wide_one - wide_fraction) : (wide_one + wide_fraction);
  auto adjusted = wide_price * factor;
  return common::Decimal::fromRaw(adjusted.raw());
}

}  // namespace

PriceBandsCalculator::PriceBandsCalculator(std::vector<common::Decimal> offsets_bps)
    : offsets_bps_(std::move(offsets_bps)) {
  if (offsets_bps_.empty()) {
    spdlog::warn("price_bands: calculator initialized with zero offsets");
    return;
  }
  spdlog::debug("price_bands: calculator using Decimal backend '{}'",
                [] {
#if defined(HERMENEUTIC_DECIMAL_BACKEND_DOUBLE)
                  return "double";
#elif defined(HERMENEUTIC_DECIMAL_BACKEND_WIDE)
                  return "wide";
#else
                  return "int128";
#endif
                }());
  std::ostringstream offsets_stream;
  offsets_stream << "price_bands: offsets=";
  for (size_t i = 0; i < offsets_bps_.size(); ++i) {
    offsets_stream << offsets_bps_[i].toString(0);
    if (i + 1 < offsets_bps_.size()) {
      offsets_stream << ",";
    }
  }
  spdlog::debug(offsets_stream.str());
}

std::vector<common::PriceBandQuote> PriceBandsCalculator::compute(
    const common::AggregatedBookView& view) const {
  common::AggregatedQuote best_bid = view.best_bid;
  common::AggregatedQuote best_ask = view.best_ask;
  if (best_bid.quantity <= kZero && !view.bid_levels.empty()) {
    best_bid.price = view.bid_levels.front().price;
    best_bid.quantity = view.bid_levels.front().quantity;
    spdlog::debug("price_bands: using bid_levels[0] as best bid");
  }
  if (best_ask.quantity <= kZero && !view.ask_levels.empty()) {
    best_ask.price = view.ask_levels.front().price;
    best_ask.quantity = view.ask_levels.front().quantity;
    spdlog::debug("price_bands: using ask_levels[0] as best ask");
  }
  const bool has_live_best = (best_bid.quantity > kZero && best_ask.quantity > kZero &&
                              best_ask.price > best_bid.price);
  spdlog::debug("price_bands: snapshot received bid_price={} bid_qty={} ask_price={} ask_qty={} live={}",
                best_bid.price.toString(8), best_bid.quantity.toString(8),
                best_ask.price.toString(8), best_ask.quantity.toString(8), has_live_best);
  if (has_live_best) {
    cached_best_bid_ = best_bid;
    cached_best_ask_ = best_ask;
    have_cached_best_ = true;
    spdlog::debug("price_bands: refreshing cache bid={} qty={} ask={} qty={}",
                  best_bid.price.toString(8), best_bid.quantity.toString(8),
                  best_ask.price.toString(8), best_ask.quantity.toString(8));
  } else if (have_cached_best_) {
    spdlog::warn("price_bands: reusing cached book (bid_qty={} ask_qty={} has_live={})",
                 best_bid.quantity.toString(8), best_ask.quantity.toString(8), has_live_best);
    best_bid = cached_best_bid_;
    best_ask = cached_best_ask_;
  } else {
    spdlog::warn("price_bands: skipping snapshot (missing bid/ask). bid_price={} qty={} ask_price={} qty={}",
                 best_bid.price.toString(8), best_bid.quantity.toString(8),
                 best_ask.price.toString(8), best_ask.quantity.toString(8));
    return {};
  }
  HERMENEUTIC_ASSERT_DEBUG(best_ask.price > best_bid.price,
                           "price band input best ask must exceed best bid");
  std::vector<common::PriceBandQuote> quotes;
  quotes.reserve(offsets_bps_.size());
  for (const auto& offset : offsets_bps_) {
    auto bid_price = adjustPrice(best_bid.price, offset, true);
    auto ask_price = adjustPrice(best_ask.price, offset, false);
    HERMENEUTIC_ASSERT_DEBUG(bid_price >= kZero, "price band bid negative");
    HERMENEUTIC_ASSERT_DEBUG(ask_price >= kZero, "price band ask negative");
    if (!(ask_price > bid_price)) {
      spdlog::error(
          "price_bands: invalid band offset={} bps best_bid={} best_ask={} bid_price={} "
          "ask_price={} fraction={}",
          offset.toString(0), best_bid.price.toString(8), best_ask.price.toString(8),
          bid_price.toString(8), ask_price.toString(8),
          (offset / kTenThousand).toString(8));
      spdlog::error("price_bands: raw offset={} raw_bid={} raw_ask={} raw_fraction={}",
                    offset.raw(), best_bid.price.raw(), best_ask.price.raw(),
                    (offset / kTenThousand).raw());
      HERMENEUTIC_ASSERT_DEBUG(false, "price band ask must exceed bid for offset");
    }
    quotes.push_back(common::PriceBandQuote{offset, bid_price, ask_price});
  }
  return quotes;
}

std::vector<common::Decimal> defaultOffsets() {
  return {
      common::Decimal::fromInteger(50),
      common::Decimal::fromInteger(100),
      common::Decimal::fromInteger(200),
      common::Decimal::fromInteger(500),
      common::Decimal::fromInteger(1000),
  };
}

std::string formatQuote(const common::PriceBandQuote& quote) {
  std::ostringstream stream;
  stream << "Offset " << quote.offset_bps.toString(0)
         << " bps -> bid " << quote.bid_price.toString(2)
         << " ask " << quote.ask_price.toString(2);
  return stream.str();
}

}  // namespace hermeneutic::price_bands
