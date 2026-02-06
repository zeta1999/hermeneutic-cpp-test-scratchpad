#pragma once

#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>

namespace hermeneutic::common {

class Decimal {
 public:
  using Storage = __int128_t;
  static constexpr Storage kScale = static_cast<Storage>(1000000000000000000LL);

  constexpr Decimal() = default;

  static constexpr Decimal fromRaw(Storage raw) {
    Decimal d;
    d.value_ = raw;
    return d;
  }

  static constexpr Decimal fromInteger(std::int64_t integer) {
    return fromRaw(static_cast<Storage>(integer) * kScale);
  }

  static Decimal fromDouble(double value);
  static Decimal fromString(std::string_view text);

  double toDouble() const;
  std::string toString(int precision = 6) const;

  Storage raw() const { return value_; }

  friend constexpr Decimal operator+(Decimal lhs, Decimal rhs) {
    return Decimal::fromRaw(lhs.value_ + rhs.value_);
  }

  friend constexpr Decimal operator-(Decimal lhs, Decimal rhs) {
    return Decimal::fromRaw(lhs.value_ - rhs.value_);
  }

  friend constexpr bool operator==(Decimal lhs, Decimal rhs) {
    return lhs.value_ == rhs.value_;
  }

  friend constexpr bool operator!=(Decimal lhs, Decimal rhs) {
    return !(lhs == rhs);
  }

  friend constexpr bool operator<(Decimal lhs, Decimal rhs) {
    return lhs.value_ < rhs.value_;
  }

  friend constexpr bool operator>(Decimal lhs, Decimal rhs) {
    return rhs < lhs;
  }

  friend constexpr bool operator<=(Decimal lhs, Decimal rhs) {
    return !(rhs < lhs);
  }

  friend constexpr bool operator>=(Decimal lhs, Decimal rhs) {
    return !(lhs < rhs);
  }

  friend Decimal operator*(Decimal lhs, Decimal rhs) {
    __int128_t wide = lhs.value_ * rhs.value_;
    return Decimal::fromRaw(static_cast<Storage>(wide / kScale));
  }

  friend Decimal operator/(Decimal lhs, Decimal rhs) {
    __int128_t wide = (lhs.value_ * kScale) / rhs.value_;
    return Decimal::fromRaw(static_cast<Storage>(wide));
  }

  Decimal& operator+=(Decimal rhs) {
    value_ += rhs.value_;
    return *this;
  }

  Decimal& operator-=(Decimal rhs) {
    value_ -= rhs.value_;
    return *this;
  }

  Decimal& operator*=(Decimal rhs) {
    *this = *this * rhs;
    return *this;
  }

  Decimal& operator/=(Decimal rhs) {
    *this = *this / rhs;
    return *this;
  }

 private:
  Storage value_{0};
};

Decimal abs(Decimal value);
Decimal min(Decimal lhs, Decimal rhs);
Decimal max(Decimal lhs, Decimal rhs);
std::ostream& operator<<(std::ostream& os, const Decimal& value);

}  // namespace hermeneutic::common

namespace std {

template <>
struct hash<hermeneutic::common::Decimal> {
  size_t operator()(const hermeneutic::common::Decimal& value) const noexcept {
    auto raw = value.raw();
    auto high = static_cast<std::uint64_t>(raw >> 64);
    auto low = static_cast<std::uint64_t>(raw & 0xffffffffffffffffULL);
    return std::hash<std::uint64_t>{}(high ^ low);
  }
};

}  // namespace std
