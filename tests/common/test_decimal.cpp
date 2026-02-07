#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cmath>
#include <stdexcept>
#include <string>

#include "hermeneutic/common/decimal.hpp"

using hermeneutic::common::DecimalDouble;
using hermeneutic::common::DecimalImplementation;
using hermeneutic::common::DecimalInt128;
using hermeneutic::common::DecimalWide;
using hermeneutic::common::abs;
using hermeneutic::common::max;
using hermeneutic::common::min;

template <typename DecimalType>
struct DecimalBehavior {
  static constexpr double tolerance = 1e-18;
  static constexpr bool expect_exact_strings = true;
};

template <>
struct DecimalBehavior<DecimalDouble> {
  static constexpr double tolerance = 1e-9;
  static constexpr bool expect_exact_strings = false;
};

template <typename DecimalType>
bool approxEqual(const DecimalType &value, double expected) {
  return std::fabs(value.toDouble() - expected) <=
         DecimalBehavior<DecimalType>::tolerance;
}

template <typename DecimalType>
void checkString(const DecimalType &value, std::string_view expected,
                 int precision) {
  if constexpr (DecimalBehavior<DecimalType>::expect_exact_strings) {
    CHECK(value.toString(precision) == expected);
  } else {
    double parsed = std::stod(std::string(expected));
    CHECK(approxEqual(value, parsed));
  }
}

template <typename DecimalType>
void runBasicMathSuite() {
  auto a = DecimalType::fromString("1.25");
  auto b = DecimalType::fromString("2.75");
  auto sum = a + b;
  checkString(sum, "4.00", 2);
  auto diff = b - a;
  checkString(diff, "1.50", 2);
  auto product = a * b;
  checkString(product, "3.437500000000000000", 18);
  auto quotient = b / a;
  CHECK(approxEqual(quotient, 2.2));
}

template <typename DecimalType>
void runFormattingSuite() {
  auto value = DecimalType::fromString("123.456789");
  CHECK(approxEqual(value, 123.456789));
  checkString(value, "123.456789", 6);
  auto zero = DecimalType::fromString("0.0");
  checkString(zero, "0.00", 2);
}

template <typename DecimalType>
void runFractionalClampSuite() {
  auto value = DecimalType::fromString("0.1234567891234567899");
  auto expected = DecimalType::fromString("0.123456789123456789");
  CHECK(approxEqual(value, expected.toDouble()));
  auto long_tail = DecimalType::fromString("42.100000000000000000");
  checkString(long_tail, "42.100000000000000000", 18);
}

template <typename DecimalType>
void runHelperSuite() {
  auto neg = DecimalType::fromString("-42.125");
  auto pos = DecimalType::fromString("42.125");
  CHECK(abs(neg) == pos);
  CHECK(min(neg, pos) == neg);
  CHECK(max(neg, pos) == pos);
  CHECK(approxEqual(neg + pos, 0.0));
}

template <typename DecimalType>
void runMulDivSuite() {
  auto lhs = DecimalType::fromString("1.234567890123456789");
  auto rhs = DecimalType::fromString("2.5");
  auto product = lhs * rhs;
  checkString(product, "3.086419725308641972", 18);
  auto quotient = product / rhs;
  CHECK(approxEqual(quotient, lhs.toDouble()));
}

template <typename DecimalType>
void runDoubleConversionSuite() {
  auto from_double = DecimalType::fromDouble(8.125);
  checkString(from_double, "8.125", 3);
  CHECK(approxEqual(from_double, 8.125));
}

template <typename DecimalType>
void runInvalidParsingSuite(const char *label) {
  auto expect_invalid = [label](std::string_view text) {
    bool threw = false;
    try {
      (void)DecimalType::fromString(text);
    } catch (const std::invalid_argument &) {
      threw = true;
    }
    if (!threw) {
      std::cerr << "expected invalid parse for " << label << " input '"
                << text << "'\n";
    }
    CHECK(threw);
  };
  expect_invalid("");
  expect_invalid("abc");
  expect_invalid("1.2.3");
}

template <typename DecimalType>
void runDecimalSuites(const char *label) {
  runBasicMathSuite<DecimalType>();
  runFormattingSuite<DecimalType>();
  runFractionalClampSuite<DecimalType>();
  runHelperSuite<DecimalType>();
  runMulDivSuite<DecimalType>();
  runDoubleConversionSuite<DecimalType>();
  runInvalidParsingSuite<DecimalType>(label);
}

TEST_CASE("decimal backend int128") {
  runDecimalSuites<DecimalInt128>("int128");
}

TEST_CASE("decimal backend wide integer") {
  runDecimalSuites<DecimalWide>("wide");
}

TEST_CASE("decimal backend floating double") {
  runDecimalSuites<DecimalDouble>("double");
}
