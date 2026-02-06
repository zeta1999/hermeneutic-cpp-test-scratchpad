#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "hermeneutic/common/decimal.hpp"

using hermeneutic::common::Decimal;

TEST_CASE("decimal basic math") {
  auto a = Decimal::fromString("1.25");
  auto b = Decimal::fromString("2.75");
  CHECK((a + b).toString(2) == "4.00");
  CHECK((b - a).toString(2) == "1.50");
  CHECK((a * b).toString(2) == "3.43");
  CHECK((b / a).toString(2) == "2.20");
}

TEST_CASE("decimal parsing and formatting") {
  auto value = Decimal::fromString("123.456789");
  CHECK(value.toString(6) == "123.456789");
  auto zero = Decimal::fromString("0.0");
  CHECK(zero.toString(2) == "0.00");
}
