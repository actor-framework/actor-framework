// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

// Based on http://beej.us/guide/bgnet/examples/pack2.c

#pragma once

#include <cmath>
#include <cstdint>
#include <limits>

namespace caf::detail {

template <class T>
struct ieee_754_trait;

template <>
struct ieee_754_trait<float> {
  static constexpr uint32_t bits = 32;                 // number of bits
  static constexpr uint32_t expbits = 8;               // bits used for exponent
  static constexpr float zero = 0.0f;                  // the value 0
  static constexpr float p5 = 0.5f;                    // the value 0.5
  static constexpr uint32_t packed_pzero = 0x00000000; // positive zero
  static constexpr uint32_t packed_nzero = 0x80000000; // negative zero
  static constexpr uint32_t packed_nan = 0xFFFFFFFF;   // not-a-number
  static constexpr uint32_t packed_pinf = 0xFF800000;  // positive infinity
  static constexpr uint32_t packed_ninf = 0x7F800000;  // negative infinity
  using packed_type = uint32_t;                        // unsigned integer type
  using signed_packed_type = int32_t;                  // signed integer type
  using float_type = float;                            // floating point type
};

template <>
struct ieee_754_trait<uint32_t> : ieee_754_trait<float> {};

template <>
struct ieee_754_trait<double> {
  static constexpr uint64_t bits = 64;
  static constexpr uint64_t expbits = 11;
  static constexpr double zero = 0.0;
  static constexpr double p5 = 0.5;
  static constexpr uint64_t packed_pzero = 0x0000000000000000ull;
  static constexpr uint64_t packed_nzero = 0x8000000000000000ull;
  static constexpr uint64_t packed_nan = 0xFFFFFFFFFFFFFFFFull;
  static constexpr uint64_t packed_pinf = 0xFFF0000000000000ull;
  static constexpr uint64_t packed_ninf = 0x7FF0000000000000ull;
  using packed_type = uint64_t;
  using signed_packed_type = int64_t;
  using float_type = double;
};

template <>
struct ieee_754_trait<uint64_t> : ieee_754_trait<double> {};

template <class T>
typename ieee_754_trait<T>::packed_type pack754(T f) {
  using trait = ieee_754_trait<T>;
  using result_type = typename trait::packed_type;
  // filter special cases
  if (std::isnan(f))
    return trait::packed_nan;
  if (std::isinf(f))
    return std::signbit(f) ? trait::packed_ninf : trait::packed_pinf;
  if (std::fabs(f) <= trait::zero) // only true if f equals +0 or -0
    return std::signbit(f) ? trait::packed_nzero : trait::packed_pzero;
  auto significandbits = trait::bits - trait::expbits - 1; // -1 for sign bit
  // check sign and begin normalization
  result_type sign;
  T fnorm;
  if (f < 0) {
    sign = 1;
    fnorm = -f;
  } else {
    sign = 0;
    fnorm = f;
  }
  // get the normalized form of f and track the exponent
  typename ieee_754_trait<T>::packed_type shift = 0;
  while (fnorm >= static_cast<T>(2)) {
    fnorm /= static_cast<T>(2);
    ++shift;
  }
  while (fnorm < static_cast<T>(1)) {
    fnorm *= static_cast<T>(2);
    --shift;
  }
  fnorm = fnorm - static_cast<T>(1);
  // calculate 2^significandbits
  auto pownum = static_cast<T>(result_type{1} << significandbits);
  // calculate the binary form (non-float) of the significand data
  auto significand = static_cast<result_type>(fnorm * (pownum + trait::p5));
  // get the biased exponent
  auto exp = shift + ((1 << (trait::expbits - 1)) - 1); // shift + bias
  // return the final answer
  return (sign << (trait::bits - 1))
         | (exp << (trait::bits - trait::expbits - 1)) | significand;
}

template <class T>
typename ieee_754_trait<T>::float_type unpack754(T i) {
  using trait = ieee_754_trait<T>;
  using signed_type = typename trait::signed_packed_type;
  using result_type = typename trait::float_type;
  using limits = std::numeric_limits<result_type>;
  switch (i) {
    case trait::packed_pzero:
      return trait::zero;
    case trait::packed_nzero:
      return -trait::zero;
    case trait::packed_pinf:
      return limits::infinity();
    case trait::packed_ninf:
      return -limits::infinity();
    case trait::packed_nan:
      return limits::quiet_NaN();
    default:
      // carry on
      break;
  }
  auto significandbits = trait::bits - trait::expbits - 1; // -1 for sign bit
  // pull the significand: mask, convert back to float + add the one back on
  auto result = static_cast<result_type>(i & ((T{1} << significandbits) - 1));
  result /= static_cast<result_type>(T{1} << significandbits);
  result += static_cast<result_type>(1);
  // deal with the exponent
  auto si = static_cast<signed_type>(i);
  auto bias = (1 << (trait::expbits - 1)) - 1;
  auto pownum = static_cast<signed_type>(1) << trait::expbits;
  auto shift
    = static_cast<signed_type>(((si >> significandbits) & (pownum - 1)) - bias);
  while (shift > 0) {
    result *= static_cast<result_type>(2);
    --shift;
  }
  while (shift < 0) {
    result /= static_cast<result_type>(2);
    ++shift;
  }
  // sign it
  result *= static_cast<result_type>(((i >> (trait::bits - 1)) & 1) ? -1 : 1);
  return result;
}

} // namespace caf::detail
