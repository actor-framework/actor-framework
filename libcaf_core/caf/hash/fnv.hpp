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
#include "caf/inspector_access.hpp"
#include "caf/save_inspector_base.hpp"
#include "caf/span.hpp"
#include "caf/string_view.hpp"
#include "caf/type_id.hpp"

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
class fnv : public save_inspector_base<fnv<T>> {
public:
  static_assert(sizeof(T) == 4 || sizeof(T) == 8);

  using super = save_inspector;

  constexpr fnv() noexcept : result(init()) {
    // nop
  }

  static constexpr bool has_human_readable_format() noexcept {
    return false;
  }

  constexpr bool begin_object(string_view) {
    return true;
  }

  constexpr bool end_object() {
    return true;
  }

  bool begin_field(string_view) {
    return true;
  }

  bool begin_field(string_view, bool is_present) {
    return value(static_cast<uint8_t>(is_present));
  }

  bool begin_field(string_view, span<const type_id_t>, size_t index) {
    return value(index);
  }

  bool begin_field(string_view, bool is_present, span<const type_id_t>,
                   size_t index) {
    value(static_cast<uint8_t>(is_present));
    if (is_present)
      value(index);
    return true;
  }

  constexpr bool end_field() {
    return true;
  }

  constexpr bool begin_tuple(size_t) {
    return true;
  }

  constexpr bool end_tuple() {
    return true;
  }

  constexpr bool begin_sequence(size_t) {
    return true;
  }

  constexpr bool end_sequence() {
    return true;
  }

  template <class Integral>
  std::enable_if_t<std::is_integral<Integral>::value, bool>
  value(Integral x) noexcept {
    auto begin = reinterpret_cast<const uint8_t*>(&x);
    append(begin, begin + sizeof(Integral));
    return true;
  }

  bool value(bool x) noexcept {
    auto tmp = static_cast<uint8_t>(x);
    return value(tmp);
  }

  bool value(float x) noexcept {
    return value(detail::pack754(x));
  }

  bool value(double x) noexcept {
    return value(detail::pack754(x));
  }

  bool value(string_view x) noexcept {
    auto begin = reinterpret_cast<const uint8_t*>(x.data());
    append(begin, begin + x.size());
    return true;
  }

  bool value(span<const byte> x) noexcept {
    auto begin = reinterpret_cast<const uint8_t*>(x.data());
    append(begin, begin + x.size());
    return true;
  }

  /// Convenience function for computing an FNV1a hash value for given
  /// arguments in one shot.
  template <class... Ts>
  static T compute(Ts&&... xs) noexcept {
    using detail::as_mutable_ref;
    fnv f;
    auto inspect_result = (inspect_object(f, as_mutable_ref(xs)) && ...);
    // Discard inspection result: always true.
    static_cast<void>(inspect_result);
    return f.result;
  }

  T result;

private:
  static constexpr T init() noexcept {
    if constexpr (sizeof(T) == 4)
      return 0x811C9DC5u;
    else
      return 0xCBF29CE484222325ull;
  }

  void append(const uint8_t* begin, const uint8_t* end) noexcept {
    if constexpr (sizeof(T) == 4)
      while (begin != end)
        result = (*begin++ ^ result) * 0x01000193u;
    else
      while (begin != end)
        result = (*begin++ ^ result) * 1099511628211ull;
  }
};

} // namespace caf::hash
