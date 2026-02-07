#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <limits>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

#include "math/wide_integer/uintwide_t.hpp"

namespace hermeneutic::common {

enum class DecimalImplementation { Int128, Double, Wide };

namespace detail {

template <typename Derived, typename Ops>
class ScaledDecimalBase {
 public:
  using Storage = typename Ops::Storage;
  using WideType = typename Ops::WideType;
  static constexpr Storage kScale = Ops::scale;
  // 10^18 fits within 60 bits (2^60 ≈ 1.15e18), so storing scaled values in
  // signed 128-bit integers leaves ample headroom.

  constexpr ScaledDecimalBase() = default;

  static constexpr Derived fromRaw(Storage raw) {
    Derived value;
    value.value_ = raw;
    return value;
  }

  static constexpr Derived fromInteger(std::int64_t integer) {
    return fromRaw(static_cast<Storage>(integer) * kScale);
  }

  static Derived fromDouble(double value) {
    long double scaled = static_cast<long double>(value) *
                         static_cast<long double>(kScale);
    auto raw = static_cast<Storage>(std::llround(scaled));
    return fromRaw(raw);
  }

  static Derived fromString(std::string_view text) {
    if (text.empty()) {
      throw std::invalid_argument("empty decimal");
    }
    bool negative = false;
    std::size_t index = 0;
    if (text[index] == '+' || text[index] == '-') {
      negative = text[index] == '-';
      ++index;
    }
    if (index >= text.size() ||
        !std::isdigit(static_cast<unsigned char>(text[index]))) {
      throw std::invalid_argument("invalid decimal format");
    }
    Storage integral = 0;
    while (index < text.size() &&
           std::isdigit(static_cast<unsigned char>(text[index]))) {
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
      while (index < text.size() &&
             std::isdigit(static_cast<unsigned char>(text[index]))) {
        if (fractional_digits < 18) {
          fractional = fractional * 10 +
                       static_cast<Storage>(text[index] - '0');
          ++fractional_digits;
        }
        ++index;
      }
      while (fractional_digits < 18) {
        fractional *= 10;
        ++fractional_digits;
      }
    }
    if (index != text.size()) {
      throw std::invalid_argument("invalid decimal format");
    }
    Storage raw = integral * kScale + fractional;
    if (negative) {
      raw = -raw;
    }
    return fromRaw(raw);
  }

  double toDouble() const {
    return static_cast<double>(static_cast<long double>(value_) /
                               static_cast<long double>(kScale));
  }

  std::string toString(int precision = 6) const {
    precision = std::clamp(precision, 0, 18);
    if (value_ == 0) {
      if (precision == 0) {
        return "0";
      }
      return "0." + std::string(static_cast<std::size_t>(precision), '0');
    }
    bool negative = value_ < 0;
    Storage abs_value = negative ? -value_ : value_;
    Storage integral = abs_value / kScale;
    Storage fractional = abs_value % kScale;

    std::string integral_str = integerToString(integral);
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

  Storage raw() const { return value_; }

  Derived &operator+=(Derived rhs) {
    value_ += rhs.value_;
    return self();
  }

  Derived &operator-=(Derived rhs) {
    value_ -= rhs.value_;
    return self();
  }

  Derived &operator*=(Derived rhs) {
    self() = self() * rhs;
    return self();
  }

  Derived &operator/=(Derived rhs) {
    self() = self() / rhs;
    return self();
  }

  friend Derived operator+(Derived lhs, Derived rhs) {
    lhs += rhs;
    return lhs;
  }

  friend Derived operator-(Derived lhs, Derived rhs) {
    lhs -= rhs;
    return lhs;
  }

  friend Derived operator*(Derived lhs, Derived rhs) {
    return multiply(lhs, rhs);
  }

  friend Derived operator/(Derived lhs, Derived rhs) {
    return divide(lhs, rhs);
  }

  friend bool operator==(Derived lhs, Derived rhs) {
    return lhs.value_ == rhs.value_;
  }

  friend bool operator!=(Derived lhs, Derived rhs) { return !(lhs == rhs); }

  friend bool operator<(Derived lhs, Derived rhs) {
    return lhs.value_ < rhs.value_;
  }

  friend bool operator>(Derived lhs, Derived rhs) { return rhs < lhs; }

  friend bool operator<=(Derived lhs, Derived rhs) { return !(rhs < lhs); }

  friend bool operator>=(Derived lhs, Derived rhs) { return !(lhs < rhs); }

 protected:
  Storage value_{0};

 private:
  Derived &self() { return static_cast<Derived &>(*this); }
  const Derived &self() const { return static_cast<const Derived &>(*this); }

  static Derived multiply(Derived lhs, Derived rhs) {
    bool negative = (lhs.value_ < 0) ^ (rhs.value_ < 0);
    auto left = magnitudeToWide(lhs.value_);
    auto right = magnitudeToWide(rhs.value_);
    auto product = left * right;
    auto scaled = product / Ops::from_storage(kScale);
    return fromWideMagnitude(scaled, negative);
  }

  static Derived divide(Derived lhs, Derived rhs) {
    if (rhs.value_ == 0) {
      throw std::domain_error("division by zero");
    }
    bool negative = (lhs.value_ < 0) ^ (rhs.value_ < 0);
    auto numerator = magnitudeToWide(lhs.value_) *
                     Ops::from_storage(kScale);
    auto denominator = magnitudeToWide(rhs.value_);
    auto quotient = numerator / denominator;
    return fromWideMagnitude(quotient, negative);
  }

  static WideType magnitudeToWide(Storage value) {
    Storage magnitude = value < 0 ? -value : value;
    return Ops::from_storage(magnitude);
  }

  static Derived fromWideMagnitude(const WideType &wide, bool negative) {
    Storage magnitude = Ops::to_storage(wide);
    Storage raw = negative ? -magnitude : magnitude;
    return fromRaw(raw);
  }

  static std::string integerToString(Storage value) {
    if (value == 0) {
      return "0";
    }
    std::string digits;
    Storage tmp = value;
    while (tmp > 0) {
      auto div = tmp / 10;
      auto mod = static_cast<int>(tmp % 10);
      digits.push_back(static_cast<char>('0' + mod));
      tmp = div;
    }
    std::reverse(digits.begin(), digits.end());
    return digits;
  }
};

struct Int128Ops {
  using Storage = __int128_t;
  using WideType = __int128_t;
  static constexpr Storage scale = static_cast<Storage>(1000000000000000000LL);

  static WideType from_storage(Storage value) { return value; }
  static Storage to_storage(WideType value) { return static_cast<Storage>(value); }
};

struct WideOps {
  using Storage = __int128_t;
  using WideType = math::wide_integer::uintwide_t<256, std::uint32_t>;
  static constexpr Storage scale = Int128Ops::scale;

  static WideType from_storage(Storage value) {
    return WideType::from_uint128(static_cast<unsigned __int128>(value));
  }

  static Storage to_storage(const WideType &value) {
    unsigned __int128 narrowed = value.to_uint128();
    return static_cast<Storage>(narrowed);
  }
};

template <typename Derived>
class DoubleDecimalBase {
 public:
  using Storage = double;

  constexpr DoubleDecimalBase() = default;

  static constexpr Derived fromRaw(double raw) {
    Derived value;
    value.value_ = raw;
    return value;
  }

  static constexpr Derived fromInteger(std::int64_t integer) {
    return fromRaw(static_cast<double>(integer));
  }

  static constexpr Derived fromDouble(double value) { return fromRaw(value); }

  static Derived fromString(std::string_view text) {
    if (text.empty()) {
      throw std::invalid_argument("empty decimal");
    }
    std::string buffer(text);
    std::size_t parsed = 0;
    double value = 0.0;
    try {
      value = std::stod(buffer, &parsed);
    } catch (const std::exception &) {
      throw std::invalid_argument("invalid decimal format");
    }
    if (parsed != buffer.size()) {
      throw std::invalid_argument("invalid decimal format");
    }
    return fromRaw(value);
  }

  double toDouble() const { return value_; }

  std::string toString(int precision = 6) const {
    precision = std::clamp(precision, 0, 18);
    std::ostringstream oss;
    oss.setf(std::ios::fixed, std::ios::floatfield);
    oss.precision(precision);
    oss << value_;
    return oss.str();
  }

  double raw() const { return value_; }

  Derived &operator+=(Derived rhs) {
    value_ += rhs.value_;
    return self();
  }

  Derived &operator-=(Derived rhs) {
    value_ -= rhs.value_;
    return self();
  }

  Derived &operator*=(Derived rhs) {
    value_ *= rhs.value_;
    return self();
  }

  Derived &operator/=(Derived rhs) {
    value_ /= rhs.value_;
    return self();
  }

  friend Derived operator+(Derived lhs, Derived rhs) {
    lhs += rhs;
    return lhs;
  }

  friend Derived operator-(Derived lhs, Derived rhs) {
    lhs -= rhs;
    return lhs;
  }

  friend Derived operator*(Derived lhs, Derived rhs) {
    lhs *= rhs;
    return lhs;
  }

  friend Derived operator/(Derived lhs, Derived rhs) {
    lhs /= rhs;
    return lhs;
  }

  friend bool operator==(Derived lhs, Derived rhs) {
    return lhs.value_ == rhs.value_;
  }

  friend bool operator!=(Derived lhs, Derived rhs) { return !(lhs == rhs); }

  friend bool operator<(Derived lhs, Derived rhs) {
    return lhs.value_ < rhs.value_;
  }

  friend bool operator>(Derived lhs, Derived rhs) { return rhs < lhs; }

  friend bool operator<=(Derived lhs, Derived rhs) { return !(rhs < lhs); }

  friend bool operator>=(Derived lhs, Derived rhs) { return !(lhs < rhs); }

 protected:
  double value_{0.0};

 private:
  Derived &self() { return static_cast<Derived &>(*this); }
  const Derived &self() const { return static_cast<const Derived &>(*this); }
};

}  // namespace detail

template <DecimalImplementation Impl>
class BasicDecimal;

template <>
class BasicDecimal<DecimalImplementation::Int128>
    : public detail::ScaledDecimalBase<
          BasicDecimal<DecimalImplementation::Int128>, detail::Int128Ops> {
 public:
  using Base = detail::ScaledDecimalBase<
      BasicDecimal<DecimalImplementation::Int128>, detail::Int128Ops>;
  using Base::Base;
};

template <>
class BasicDecimal<DecimalImplementation::Wide>
    : public detail::ScaledDecimalBase<
          BasicDecimal<DecimalImplementation::Wide>, detail::WideOps> {
 public:
  using Base = detail::ScaledDecimalBase<
      BasicDecimal<DecimalImplementation::Wide>, detail::WideOps>;
  using Base::Base;
};

template <>
class BasicDecimal<DecimalImplementation::Double>
    : public detail::DoubleDecimalBase<
          BasicDecimal<DecimalImplementation::Double>> {
 public:
  using Base =
      detail::DoubleDecimalBase<BasicDecimal<DecimalImplementation::Double>>;
  using Base::Base;
};

using DecimalInt128 = BasicDecimal<DecimalImplementation::Int128>;
using DecimalWide = BasicDecimal<DecimalImplementation::Wide>;
using DecimalDouble = BasicDecimal<DecimalImplementation::Double>;

#if defined(HERMENEUTIC_DECIMAL_BACKEND_DOUBLE)
constexpr DecimalImplementation kDefaultDecimalImplementation =
    DecimalImplementation::Double;
#elif defined(HERMENEUTIC_DECIMAL_BACKEND_WIDE)
constexpr DecimalImplementation kDefaultDecimalImplementation =
    DecimalImplementation::Wide;
#else
constexpr DecimalImplementation kDefaultDecimalImplementation =
    DecimalImplementation::Int128;
#endif

using DecimalDefault = BasicDecimal<kDefaultDecimalImplementation>;
using Decimal = DecimalDefault;

template <DecimalImplementation Impl>
BasicDecimal<Impl> abs(BasicDecimal<Impl> value) {
  return value.raw() < 0 ? BasicDecimal<Impl>::fromRaw(-value.raw()) : value;
}

template <DecimalImplementation Impl>
BasicDecimal<Impl> min(BasicDecimal<Impl> lhs, BasicDecimal<Impl> rhs) {
  return lhs < rhs ? lhs : rhs;
}

template <DecimalImplementation Impl>
BasicDecimal<Impl> max(BasicDecimal<Impl> lhs, BasicDecimal<Impl> rhs) {
  return lhs < rhs ? rhs : lhs;
}

template <DecimalImplementation Impl>
std::ostream &operator<<(std::ostream &os, const BasicDecimal<Impl> &value) {
  return os << value.toString(6);
}

}  // namespace hermeneutic::common

namespace std {

template <hermeneutic::common::DecimalImplementation Impl>
struct hash<hermeneutic::common::BasicDecimal<Impl>> {
  std::size_t operator()(const hermeneutic::common::BasicDecimal<Impl> &value) const noexcept {
    using Storage = decltype(value.raw());
    if constexpr (std::is_same_v<Storage, double>) {
      return std::hash<double>{}(value.raw());
    } else {
      auto raw = value.raw();
      auto high = static_cast<std::uint64_t>(raw >> 64);
      auto low = static_cast<std::uint64_t>(raw & 0xffffffffffffffffULL);
      return std::hash<std::uint64_t>{}(high ^ low);
    }
  }
};

}  // namespace std
