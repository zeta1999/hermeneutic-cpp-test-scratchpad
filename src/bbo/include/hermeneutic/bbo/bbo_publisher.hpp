#pragma once

#include <string>

#include "hermeneutic/common/events.hpp"

namespace hermeneutic::bbo {

class BboPublisher {
 public:
  std::string format(const common::AggregatedBookView& view) const;
};

}  // namespace hermeneutic::bbo
