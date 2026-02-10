#include "hermeneutic/bbo/bbo_publisher.hpp"

#include <sstream>

namespace hermeneutic::bbo {

std::string BboPublisher::format(const common::AggregatedBookView& view) const {
  std::ostringstream stream;
  stream << "BEST_BID=" << view.best_bid.price.toString(8)
         << "@" << view.best_bid.quantity.toString(8)
         << " BEST_ASK=" << view.best_ask.price.toString(8)
         << "@" << view.best_ask.quantity.toString(8)
         << " EXCHANGES=" << view.exchange_count;
  return stream.str();
}

}  // namespace hermeneutic::bbo
