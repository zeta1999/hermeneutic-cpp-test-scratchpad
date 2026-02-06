#include "hermeneutic/bbo/bbo_publisher.hpp"

#include <sstream>

namespace hermeneutic::bbo {

std::string BboPublisher::format(const common::AggregatedBookView& view) const {
  std::ostringstream stream;
  stream << "BEST_BID=" << view.best_bid.price << "@" << view.best_bid.quantity
         << " BEST_ASK=" << view.best_ask.price << "@" << view.best_ask.quantity
         << " EXCHANGES=" << view.exchange_count;
  return stream.str();
}

}  // namespace hermeneutic::bbo
