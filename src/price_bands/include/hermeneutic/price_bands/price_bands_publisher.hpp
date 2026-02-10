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
};

std::vector<common::Decimal> defaultOffsets();
std::string formatQuote(const common::PriceBandQuote& quote);

}  // namespace hermeneutic::price_bands
