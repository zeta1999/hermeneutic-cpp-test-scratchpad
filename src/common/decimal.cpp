#include "hermeneutic/common/decimal.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>
#include <string>

namespace hermeneutic::common {
namespace {

std::string int128ToString(Decimal::Storage value) {
  if (value == 0) {
    return "0";
  }

  bool negative = value < 0;
  Decimal::Storage tmp = negative ? -value : value;
  std::string digits;
  while (tmp > 0) {
    auto div = tmp / 10;
    auto mod = static_cast<int>(tmp % 10);
    digits.push_back(static_cast<char>('0' + mod));
    tmp = div;
  }
  if (negative) {
    digits.push_back('-');
  }
  std::reverse(digits.begin(), digits.end());
  return digits;
}

}  // namespace

Decimal Decimal::fromDouble(double value) {
  long double scaled = static_cast<long double>(value) * static_cast<long double>(kScale);
  auto raw = static_cast<Storage>(std::llround(scaled));
  return Decimal::fromRaw(raw);
}

Decimal Decimal::fromString(std::string_view text) {
  if (text.empty()) {
    throw std::invalid_argument("empty decimal");
  }
  bool negative = false;
  size_t index = 0;
  if (text[index] == '+' || text[index] == '-') {
    negative = text[index] == '-';
    ++index;
  }

  Storage integral = 0;
  if (index >= text.size() || !std::isdigit(text[index])) {
    throw std::invalid_argument("invalid decimal format");
  }
  while (index < text.size() && std::isdigit(text[index])) {
    integral = integral * 10 + static_cast<Storage>(text[index] - '0');
    ++index;
  }

  Storage fractional = 0;
  int fractional_digits = 0;
  if (index < text.size()) {
    if (text[index] != '.') {
      throw std::invalid_argument("invalid decimal format");
    }
    ++index;
    while (index < text.size() && std::isdigit(text[index]) && fractional_digits < 18) {
      fractional = fractional * 10 + static_cast<Storage>(text[index] - '0');
      ++index;
      ++fractional_digits;
    }
    while (fractional_digits < 18) {
      fractional *= 10;
      ++fractional_digits;
    }
  }

  Storage raw = integral * kScale + fractional;
  if (negative) {
    raw = -raw;
  }
  return Decimal::fromRaw(raw);
}

double Decimal::toDouble() const {
  return static_cast<double>(static_cast<long double>(value_) /
                             static_cast<long double>(kScale));
}

std::string Decimal::toString(int precision) const {
  precision = std::clamp(precision, 0, 18);
  if (value_ == 0) {
    return precision == 0 ? std::string("0")
                           : std::string("0.") + std::string(static_cast<std::size_t>(precision), '0');
  }
  bool negative = value_ < 0;
  Storage abs_value = negative ? -value_ : value_;
  Storage integral = abs_value / kScale;
  Storage fractional = abs_value % kScale;

  std::string integral_str = int128ToString(integral);

  if (precision == 0) {
    return negative ? "-" + integral_str : integral_str;
  }

  std::string fractional_str(18, '0');
  for (int i = 17; i >= 0; --i) {
    fractional_str[static_cast<std::size_t>(i)] =
        static_cast<char>('0' + static_cast<int>(fractional % 10));
    fractional /= 10;
  }
  if (precision < 18) {
    fractional_str.resize(static_cast<std::size_t>(precision));
  }
  std::string result = integral_str;
  result.push_back('.');
  result += fractional_str;
  if (negative) {
    result.insert(result.begin(), '-');
  }
  return result;
}

Decimal abs(Decimal value) {
  return value < Decimal::fromRaw(0) ? Decimal::fromRaw(-value.raw()) : value;
}

Decimal min(Decimal lhs, Decimal rhs) {
  return lhs < rhs ? lhs : rhs;
}

Decimal max(Decimal lhs, Decimal rhs) {
  return lhs < rhs ? rhs : lhs;
}

std::ostream& operator<<(std::ostream& os, const Decimal& value) {
  os << value.toString(6);
  return os;
}

}  // namespace hermeneutic::common
