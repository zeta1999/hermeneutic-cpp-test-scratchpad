#include "hermeneutic/volume_bands/volume_bands_publisher.hpp"

#include <sstream>

namespace hermeneutic::volume_bands {

VolumeBandsCalculator::VolumeBandsCalculator(std::vector<common::Decimal> thresholds)
    : thresholds_(std::move(thresholds)) {}

std::vector<common::VolumeBandQuote> VolumeBandsCalculator::compute(
    const common::AggregatedBookView& view) const {
  std::vector<common::VolumeBandQuote> quotes;
  quotes.reserve(thresholds_.size());
  for (const auto& threshold : thresholds_) {
    auto bid_notional = view.best_bid.price * view.best_bid.quantity;
    auto ask_notional = view.best_ask.price * view.best_ask.quantity;
    common::Decimal bid_price = bid_notional >= threshold ? view.best_bid.price : common::Decimal::fromRaw(0);
    common::Decimal ask_price = ask_notional >= threshold ? view.best_ask.price : common::Decimal::fromRaw(0);
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
