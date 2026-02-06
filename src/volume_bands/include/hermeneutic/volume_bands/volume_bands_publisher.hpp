#pragma once

#include <vector>

#include "hermeneutic/common/events.hpp"

namespace hermeneutic::volume_bands {

class VolumeBandsCalculator {
 public:
  explicit VolumeBandsCalculator(std::vector<common::Decimal> thresholds);

  std::vector<common::VolumeBandQuote> compute(const common::AggregatedBookView& view) const;

 private:
  std::vector<common::Decimal> thresholds_;
};

std::vector<common::Decimal> defaultThresholds();

}  // namespace hermeneutic::volume_bands
