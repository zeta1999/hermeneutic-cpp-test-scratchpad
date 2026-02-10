#include "hermeneutic/volume_bands/volume_bands_publisher.hpp"

#include <sstream>

namespace hermeneutic::volume_bands {

VolumeBandsCalculator::VolumeBandsCalculator(std::vector<common::Decimal> thresholds)
    : thresholds_(std::move(thresholds)) {}

namespace {
inline common::DecimalWide widen(const common::Decimal& value) {
  return common::DecimalWide::fromRaw(value.raw());
}

common::Decimal findPriceForThreshold(const std::vector<common::PriceLevel>& levels,
                                      const common::Decimal& threshold) {
  common::DecimalWide accumulated = common::DecimalWide::fromRaw(0);
  const auto wide_threshold = widen(threshold);
  for (const auto& level : levels) {
    accumulated += widen(level.price) * widen(level.quantity);
    if (accumulated >= wide_threshold) {
      return level.price;
    }
  }
  return common::Decimal::fromRaw(0);
}
}  // namespace

std::vector<common::VolumeBandQuote> VolumeBandsCalculator::compute(
    const common::AggregatedBookView& view) const {
  std::vector<common::VolumeBandQuote> quotes;
  quotes.reserve(thresholds_.size());
  for (const auto& threshold : thresholds_) {
    const auto bid_price = findPriceForThreshold(view.bid_levels, threshold);
    const auto ask_price = findPriceForThreshold(view.ask_levels, threshold);
    quotes.push_back(common::VolumeBandQuote{threshold, bid_price, ask_price});
  }
  return quotes;
}

std::vector<common::Decimal> defaultThresholds() {
  return {
      common::Decimal::fromInteger(1'000'000),
      common::Decimal::fromInteger(5'000'000),
      common::Decimal::fromInteger(10'000'000),
      common::Decimal::fromInteger(25'000'000),
      common::Decimal::fromInteger(50'000'000),
  };
}

std::string formatQuote(const common::VolumeBandQuote& quote) {
  std::ostringstream stream;
  stream << "Bands " << quote.notional.toString(0)
         << " -> bid " << quote.bid_price.toString(2)
         << " ask " << quote.ask_price.toString(2);
  return stream.str();
}

}  // namespace hermeneutic::volume_bands
