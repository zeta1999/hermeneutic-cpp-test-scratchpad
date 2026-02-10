#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "hermeneutic/bbo/bbo_publisher.hpp"
#include "hermeneutic/common/events.hpp"

TEST_CASE("BboPublisher formats stdout line") {
  hermeneutic::common::AggregatedBookView view;
  view.best_bid.price = hermeneutic::common::Decimal::fromString("30000.50");
  view.best_bid.quantity = hermeneutic::common::Decimal::fromString("1.5");
  view.best_ask.price = hermeneutic::common::Decimal::fromString("30002.25");
  view.best_ask.quantity = hermeneutic::common::Decimal::fromString("2.0");
  view.exchange_count = 3;

  hermeneutic::bbo::BboPublisher publisher;
  auto line = publisher.format(view);
  CHECK(line == "BEST_BID=30000.50000000@1.50000000 BEST_ASK=30002.25000000@2.00000000 EXCHANGES=3");
}
