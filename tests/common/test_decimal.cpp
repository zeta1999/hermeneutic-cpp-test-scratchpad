#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cmath>
#include <stdexcept>

#include "hermeneutic/common/decimal.hpp"

using hermeneutic::common::Decimal;
using hermeneutic::common::abs;
using hermeneutic::common::max;
using hermeneutic::common::min;

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

TEST_CASE("decimal fractional precision clamps at 18 digits") {
  auto value = Decimal::fromString("0.1234567891234567899");
  CHECK(value.toString(18) == "0.123456789123456789");
  auto long_tail = Decimal::fromString("42.100000000000000000");
  CHECK(long_tail.toString(18) == "42.100000000000000000");
}

TEST_CASE("decimal handles negative values and helpers") {
  auto neg = Decimal::fromString("-42.125");
  auto pos = Decimal::fromString("42.125");
  CHECK(abs(neg) == pos);
  CHECK(min(neg, pos) == neg);
  CHECK(max(neg, pos) == pos);
  CHECK((neg + pos).toString(3) == "0.000");
}

TEST_CASE("decimal multiplication and division retain precision") {
  auto lhs = Decimal::fromString("1.234567890123456789");
  auto rhs = Decimal::fromString("2.5");
  auto product = lhs * rhs;
  CHECK(product.toString(18) == "3.086419725308641972");
  auto quotient = product / rhs;
  CHECK(std::fabs((quotient - lhs).toDouble()) <= 1e-18);
}

TEST_CASE("decimal double round trip") {
  auto from_double = Decimal::fromDouble(8.125);
  CHECK(from_double.toString(3) == "8.125");
  double round_trip = from_double.toDouble();
  CHECK(std::fabs(round_trip - 8.125) < 1e-12);
}
