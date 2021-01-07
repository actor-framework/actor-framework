// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstdint>

namespace caf::detail {

template <int, bool>
struct select_integer_type;

template <>
struct select_integer_type<1, true> {
  using type = int8_t;
};

template <>
struct select_integer_type<1, false> {
  using type = uint8_t;
};

template <>
struct select_integer_type<2, true> {
  using type = int16_t;
};

template <>
struct select_integer_type<2, false> {
  using type = uint16_t;
};

template <>
struct select_integer_type<4, true> {
  using type = int32_t;
};

template <>
struct select_integer_type<4, false> {
  using type = uint32_t;
};

template <>
struct select_integer_type<8, true> {
  using type = int64_t;
};

template <>
struct select_integer_type<8, false> {
  using type = uint64_t;
};

template <int Size, bool IsSigned>
using select_integer_type_t =
  typename select_integer_type<Size, IsSigned>::type;

} // namespace caf::detail
