#include "hermeneutic/price_bands/price_bands_publisher.hpp"

namespace hermeneutic::price_bands {
namespace {
const common::Decimal kOne = common::Decimal::fromInteger(1);
const common::Decimal kTenThousand = common::Decimal::fromInteger(10'000);

common::Decimal adjustPrice(const common::Decimal& price, const common::Decimal& offset_bps, bool bid) {
  auto fraction = offset_bps / kTenThousand;
  common::Decimal factor = bid ? (kOne - fraction) : (kOne + fraction);
  return price * factor;
}

}  // namespace

PriceBandsCalculator::PriceBandsCalculator(std::vector<common::Decimal> offsets_bps)
    : offsets_bps_(std::move(offsets_bps)) {}

std::vector<common::PriceBandQuote> PriceBandsCalculator::compute(
    const common::AggregatedBookView& view) const {
  std::vector<common::PriceBandQuote> quotes;
  quotes.reserve(offsets_bps_.size());
  for (const auto& offset : offsets_bps_) {
    quotes.push_back(common::PriceBandQuote{
        offset,
        adjustPrice(view.best_bid.price, offset, true),
        adjustPrice(view.best_ask.price, offset, false),
    });
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

}  // namespace hermeneutic::price_bands
