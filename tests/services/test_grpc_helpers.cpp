#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "services/common/grpc_helpers.hpp"

using hermeneutic::common::AggregatedBookView;
using hermeneutic::common::Decimal;
using hermeneutic::services::grpc_helpers::FromDomain;
using hermeneutic::services::grpc_helpers::ToDomain;

TEST_CASE("grpc helpers round-trip depth levels") {
  AggregatedBookView view;
  view.best_bid.price = Decimal::fromString("100.00");
  view.best_bid.quantity = Decimal::fromString("5");
  view.best_ask.price = Decimal::fromString("101.00");
  view.best_ask.quantity = Decimal::fromString("3");
  view.bid_levels.push_back({Decimal::fromString("100.00"), Decimal::fromString("5")});
  view.bid_levels.push_back({Decimal::fromString("99.50"), Decimal::fromString("2")});
  view.ask_levels.push_back({Decimal::fromString("101.00"), Decimal::fromString("3")});
  view.ask_levels.push_back({Decimal::fromString("101.50"), Decimal::fromString("4")});
  view.exchange_count = 2;
  view.timestamp = std::chrono::system_clock::now();

  auto proto = FromDomain(view);
  auto round_trip = ToDomain(proto);
  CHECK(round_trip.bid_levels.size() == view.bid_levels.size());
  CHECK(round_trip.ask_levels.size() == view.ask_levels.size());
  CHECK(round_trip.bid_levels[1].price.toString(2) == "99.50");
  CHECK(round_trip.ask_levels[1].quantity.toString(0) == "4");
}
