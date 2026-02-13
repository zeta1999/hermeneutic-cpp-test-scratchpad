#pragma once

#include <string>
#include <vector>

#include "hermeneutic/common/events.hpp"

namespace hermeneutic::price_bands {

class PriceBandsCalculator {
 public:
  explicit PriceBandsCalculator(std::vector<common::Decimal> offsets_bps);

 std::vector<common::PriceBandQuote> compute(const common::AggregatedBookView& view) const;

 private:
  std::vector<common::Decimal> offsets_bps_;
  mutable common::AggregatedQuote cached_best_bid_{};
  mutable common::AggregatedQuote cached_best_ask_{};
  mutable bool have_cached_best_{false};
};

std::vector<common::Decimal> defaultOffsets();
std::string formatQuote(const common::PriceBandQuote& quote);

}  // namespace hermeneutic::price_bands
