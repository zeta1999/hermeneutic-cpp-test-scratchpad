#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace math {
namespace wide_integer {

// Minimal uintwide_t implementation sufficient for 256-bit integer arithmetic.
// This header is intentionally lightweight – it only supports the operations
// required by the hermeneutic Decimal wide backend (addition, subtraction,
// multiplication, division, shifts, comparisons, and conversion to/from
// 128-bit integers). The interface mirrors ckormanyos/wide-integer's
// uintwide_t so existing code can switch to the full library without further
// changes.

template <std::size_t Bits, typename LimbType = std::uint32_t>
class uintwide_t {
  static_assert(Bits > 0, "uintwide_t requires a positive bit width");
  static_assert(std::is_unsigned_v<LimbType>, "LimbType must be unsigned");
  static_assert(std::numeric_limits<LimbType>::digits == 32,
                "This uintwide_t implementation expects 32-bit limbs");

 public:
  using limb_type = LimbType;
  static constexpr std::size_t limb_bits = std::numeric_limits<limb_type>::digits;
  static constexpr std::size_t limb_count = (Bits + limb_bits - 1) / limb_bits;
  static constexpr std::size_t num_bits = limb_count * limb_bits;

  uintwide_t() = default;

  explicit uintwide_t(unsigned __int128 value) { assign_uint128(value); }

  template <typename Integral,
            typename = std::enable_if_t<std::is_integral_v<Integral>>>
  explicit uintwide_t(Integral value) {
    if constexpr (std::is_signed_v<Integral>) {
      if (value < 0) {
        throw std::invalid_argument("uintwide_t cannot hold negative values");
      }
    }
    assign_uint128(static_cast<unsigned __int128>(value));
  }

  static uintwide_t from_uint128(unsigned __int128 value) {
    return uintwide_t(value);
  }

  static uintwide_t zero() { return uintwide_t(); }

  bool is_zero() const {
    for (auto limb : limbs_) {
      if (limb != 0) {
        return false;
      }
    }
    return true;
  }

  uintwide_t &operator+=(const uintwide_t &rhs) {
    std::uint64_t carry = 0;
    for (std::size_t i = 0; i < limb_count; ++i) {
      std::uint64_t sum = static_cast<std::uint64_t>(limbs_[i]) +
                          static_cast<std::uint64_t>(rhs.limbs_[i]) + carry;
      limbs_[i] = static_cast<limb_type>(sum & limb_mask());
      carry = sum >> limb_bits;
    }
    return *this;
  }

  uintwide_t &operator-=(const uintwide_t &rhs) {
    std::int64_t borrow = 0;
    for (std::size_t i = 0; i < limb_count; ++i) {
      std::int64_t diff = static_cast<std::int64_t>(limbs_[i]) -
                          static_cast<std::int64_t>(rhs.limbs_[i]) - borrow;
      if (diff < 0) {
        diff += static_cast<std::int64_t>(limb_mask()) + 1;
        borrow = 1;
      } else {
        borrow = 0;
      }
      limbs_[i] = static_cast<limb_type>(diff);
    }
    if (borrow != 0) {
      throw std::underflow_error("uintwide_t subtraction underflow");
    }
    return *this;
  }

  uintwide_t &operator<<=(unsigned shift) {
    if (shift == 0 || is_zero()) {
      return *this;
    }
    if (shift >= num_bits) {
      limbs_.fill(0);
      return *this;
    }
    unsigned limb_shift = shift / limb_bits;
    unsigned bit_shift = shift % limb_bits;
    if (limb_shift > 0) {
      for (std::size_t i = limb_count; i-- > 0;) {
        if (i >= limb_shift) {
          limbs_[i] = limbs_[i - limb_shift];
        } else {
          limbs_[i] = 0;
        }
      }
    }
    if (bit_shift != 0) {
      std::uint64_t carry = 0;
      for (std::size_t i = 0; i < limb_count; ++i) {
        std::uint64_t current = static_cast<std::uint64_t>(limbs_[i]);
        std::uint64_t next = (current << bit_shift) | carry;
        limbs_[i] = static_cast<limb_type>(next & limb_mask());
        carry = next >> limb_bits;
      }
    }
    return *this;
  }

  uintwide_t &operator>>=(unsigned shift) {
    if (shift == 0 || is_zero()) {
      return *this;
    }
    if (shift >= num_bits) {
      limbs_.fill(0);
      return *this;
    }
    unsigned limb_shift = shift / limb_bits;
    unsigned bit_shift = shift % limb_bits;
    if (limb_shift > 0) {
      for (std::size_t i = 0; i < limb_count; ++i) {
        if (i + limb_shift < limb_count) {
          limbs_[i] = limbs_[i + limb_shift];
        } else {
          limbs_[i] = 0;
        }
      }
    }
    if (bit_shift != 0) {
      std::uint64_t carry = 0;
      for (std::size_t i = limb_count; i-- > 0;) {
        std::uint64_t current = static_cast<std::uint64_t>(limbs_[i]);
        std::uint64_t next = (current >> bit_shift) | carry;
        limbs_[i] = static_cast<limb_type>(next & limb_mask());
        carry = (current << (limb_bits - bit_shift)) & limb_mask();
      }
    }
    return *this;
  }

  uintwide_t &operator*=(const uintwide_t &rhs) {
    std::array<unsigned __int128, limb_count * 2> temp{};
    temp.fill(0);
    for (std::size_t i = 0; i < limb_count; ++i) {
      unsigned __int128 carry = 0;
      for (std::size_t j = 0; j < limb_count; ++j) {
        std::size_t idx = i + j;
        unsigned __int128 product = static_cast<unsigned __int128>(limbs_[i]) *
                                    static_cast<unsigned __int128>(rhs.limbs_[j]);
        unsigned __int128 acc = temp[idx] + product + carry;
        temp[idx] = acc & limb_mask();
        carry = acc >> limb_bits;
      }
      temp[i + limb_count] += carry;
    }
    for (std::size_t i = 0; i < limb_count; ++i) {
      limbs_[i] = static_cast<limb_type>(temp[i] & limb_mask());
    }
    return *this;
  }

  uintwide_t &operator/=(const uintwide_t &divisor) {
    if (divisor.is_zero()) {
      throw std::domain_error("division by zero");
    }
    uintwide_t dividend = *this;
    uintwide_t quotient;
    uintwide_t remainder;
    for (std::size_t bit = num_bits; bit-- > 0;) {
      remainder <<= 1;
      if (dividend.test_bit(bit)) {
        remainder.limbs_[0] |= 1U;
      }
      if (remainder >= divisor) {
        remainder -= divisor;
        quotient.set_bit(bit);
      }
    }
    *this = quotient;
    return *this;
  }

  friend uintwide_t operator+(uintwide_t lhs, const uintwide_t &rhs) {
    lhs += rhs;
    return lhs;
  }

  friend uintwide_t operator-(uintwide_t lhs, const uintwide_t &rhs) {
    lhs -= rhs;
    return lhs;
  }

  friend uintwide_t operator*(uintwide_t lhs, const uintwide_t &rhs) {
    lhs *= rhs;
    return lhs;
  }

  friend uintwide_t operator/(uintwide_t lhs, const uintwide_t &rhs) {
    lhs /= rhs;
    return lhs;
  }

  bool test_bit(std::size_t bit) const {
    if (bit >= num_bits) {
      return false;
    }
    std::size_t limb = bit / limb_bits;
    unsigned offset = bit % limb_bits;
    return (limbs_[limb] >> offset) & 1U;
  }

  void set_bit(std::size_t bit) {
    if (bit >= num_bits) {
      throw std::out_of_range("bit index out of range");
    }
    std::size_t limb = bit / limb_bits;
    unsigned offset = bit % limb_bits;
    limbs_[limb] |= static_cast<limb_type>(1U) << offset;
  }

  unsigned __int128 to_uint128() const {
    unsigned __int128 value = 0;
    const std::size_t limit = std::min<std::size_t>(limb_count, 4);
    for (std::size_t i = 0; i < limit; ++i) {
      std::size_t idx = limit - 1 - i;
      value <<= limb_bits;
      value |= static_cast<unsigned __int128>(limbs_[idx]);
    }
    for (std::size_t i = limit; i < limb_count; ++i) {
      if (limbs_[i] != 0) {
        throw std::overflow_error("value exceeds 128 bits");
      }
    }
    return value;
  }

  friend bool operator==(const uintwide_t &lhs, const uintwide_t &rhs) {
    return lhs.limbs_ == rhs.limbs_;
  }

  friend bool operator!=(const uintwide_t &lhs, const uintwide_t &rhs) {
    return !(lhs == rhs);
  }

  friend bool operator<(const uintwide_t &lhs, const uintwide_t &rhs) {
    for (std::size_t i = limb_count; i-- > 0;) {
      if (lhs.limbs_[i] < rhs.limbs_[i]) {
        return true;
      }
      if (lhs.limbs_[i] > rhs.limbs_[i]) {
        return false;
      }
    }
    return false;
  }

  friend bool operator>(const uintwide_t &lhs, const uintwide_t &rhs) {
    return rhs < lhs;
  }

  friend bool operator<=(const uintwide_t &lhs, const uintwide_t &rhs) {
    return !(rhs < lhs);
  }

  friend bool operator>=(const uintwide_t &lhs, const uintwide_t &rhs) {
    return !(lhs < rhs);
  }

 private:
  std::array<limb_type, limb_count> limbs_{};

  static constexpr std::uint64_t limb_mask() {
    return (std::uint64_t{1} << limb_bits) - 1;
  }

  void assign_uint128(unsigned __int128 value) {
    for (std::size_t i = 0; i < limb_count; ++i) {
      limbs_[i] = static_cast<limb_type>(value & limb_mask());
      value >>= limb_bits;
    }
    if (value != 0) {
      throw std::overflow_error("value exceeds supported width");
    }
  }
};

}  // namespace wide_integer
}  // namespace math
