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
  auto fraction = offset_bps / kTenThousand;
  auto wide_price = widen(price);
  auto wide_fraction = widen(fraction);
  auto wide_one = widen(kOne);
  common::DecimalWide factor = bid ? (wide_one - wide_fraction) : (wide_one + wide_fraction);
  auto adjusted = wide_price * factor;
  return common::Decimal::fromRaw(adjusted.raw());
}

}  // namespace

PriceBandsCalculator::PriceBandsCalculator(std::vector<common::Decimal> offsets_bps)
    : offsets_bps_(std::move(offsets_bps)) {}

std::vector<common::PriceBandQuote> PriceBandsCalculator::compute(
    const common::AggregatedBookView& view) const {
  if (view.best_bid.quantity <= kZero || view.best_ask.quantity <= kZero) {
    return {};
  }
  HERMENEUTIC_ASSERT_DEBUG(view.best_ask.price > view.best_bid.price,
                           "price band input view best ask must exceed best bid");
  std::vector<common::PriceBandQuote> quotes;
  quotes.reserve(offsets_bps_.size());
  for (const auto& offset : offsets_bps_) {
    auto bid_price = adjustPrice(view.best_bid.price, offset, true);
    auto ask_price = adjustPrice(view.best_ask.price, offset, false);
    HERMENEUTIC_ASSERT_DEBUG(bid_price >= kZero, "price band bid negative");
    HERMENEUTIC_ASSERT_DEBUG(ask_price >= kZero, "price band ask negative");
    HERMENEUTIC_ASSERT_DEBUG(ask_price > bid_price,
                             "price band ask must exceed bid for offset");
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
