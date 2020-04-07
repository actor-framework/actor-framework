/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <cstdint>
#include <type_traits>

#include "caf/detail/ieee_754.hpp"
#include "caf/read_inspector.hpp"
#include "caf/span.hpp"
#include "caf/string_view.hpp"

namespace caf::hash {

/// Non-cryptographic hash algorithm (variant 1a) named after Glenn Fowler,
/// Landon Curt Noll, and Kiem-Phong Vo.
///
/// For more details regarding the public domain algorithm, see:
/// - https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
/// - http://www.isthe.com/chongo/tech/comp/fnv/index.html
///
/// @tparam T One of `uint32_t`, `uint64_t`, or `size_t`.
template <class T>
class fnv : public read_inspector<fnv<T>> {
public:
  static_assert(sizeof(T) == 4 || sizeof(T) == 8);

  using result_type = void;

  constexpr fnv() noexcept : value(init()) {
    // nop
  }

  template <class Integral>
  std::enable_if_t<std::is_integral<Integral>::value>
  apply(Integral x) noexcept {
    auto begin = reinterpret_cast<const uint8_t*>(&x);
    append(begin, begin + sizeof(Integral));
  }

  void apply(bool x) noexcept {
    auto tmp = static_cast<uint8_t>(x);
    apply(tmp);
  }

  void apply(float x) noexcept {
    apply(detail::pack754(x));
  }

  void apply(double x) noexcept {
    apply(detail::pack754(x));
  }

  void apply(string_view x) noexcept {
    auto begin = reinterpret_cast<const uint8_t*>(x.data());
    append(begin, begin + x.size());
  }

  void apply(span<const byte> x) noexcept {
    auto begin = reinterpret_cast<const uint8_t*>(x.data());
    append(begin, begin + x.size());
  }

  template <class Enum>
  std::enable_if_t<std::is_enum<Enum>::value> apply(Enum x) noexcept {
    return apply(static_cast<std::underlying_type_t<Enum>>(x));
  }

  void begin_sequence(size_t) {
    // nop
  }

  void end_sequence() {
    // nop
  }

  /// Convenience function for computing an FNV1a hash value for given
  /// arguments in one shot.
  template <class... Ts>
  static T compute(Ts&&... xs) {
    fnv f;
    f(std::forward<Ts>(xs)...);
    return f.value;
  }

  T value;

private:
  static constexpr T init() {
    if constexpr (sizeof(T) == 4)
      return 0x811C9DC5u;
    else
      return 0xCBF29CE484222325ull;
  }

  void append(const uint8_t* begin, const uint8_t* end) {
    if constexpr (sizeof(T) == 4)
      while (begin != end)
        value = (*begin++ ^ value) * 0x01000193u;
    else
      while (begin != end)
        value = (*begin++ ^ value) * 1099511628211ull;
  }
};

} // namespace caf::hash
