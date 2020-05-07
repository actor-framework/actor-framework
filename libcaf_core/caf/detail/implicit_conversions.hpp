/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
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

#include <string>
#include <type_traits>

#include "caf/actor_traits.hpp"
#include "caf/detail/squashed_int.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/error.hpp"
#include "caf/fwd.hpp"

namespace caf {
namespace detail {

constexpr int integral_type_flag = 0x01;
constexpr int error_type_flag = 0x02;
constexpr int dynamically_typed_actor_flag = 0x04;
constexpr int statically_typed_actor_flag = 0x08;

template <bool, int>
struct conversion_flag {
  static constexpr int value = 0;
};

template <int Value>
struct conversion_flag<true, Value> {
  static constexpr int value = Value;
};

template <class T>
struct implicit_conversion_oracle {
  static constexpr int value
    = conversion_flag<std::is_integral<T>::value, integral_type_flag>::value
      | conversion_flag<std::is_convertible<T, error>::value,
                        error_type_flag>::value
      | conversion_flag<std::is_base_of<dynamically_typed_actor_base, T>::value,
                        dynamically_typed_actor_flag>::value
      | conversion_flag<std::is_base_of<statically_typed_actor_base, T>::value,
                        statically_typed_actor_flag>::value;
};

template <class T, int = implicit_conversion_oracle<T>::value>
struct implicit_conversions;

template <class T>
struct implicit_conversions<T, 0> {
  using type = T;
};

template <>
struct implicit_conversions<bool, integral_type_flag> {
  using type = bool;
};

template <class T>
struct implicit_conversions<T, integral_type_flag> {
  using type = squashed_int_t<T>;
};

template <class T>
struct implicit_conversions<T, dynamically_typed_actor_flag> {
  using type = actor;
};

template <class T>
struct implicit_conversions<T, statically_typed_actor_flag> {
  using type =
    typename detail::tl_apply<typename T::signatures, typed_actor>::type;
};

template <>
struct implicit_conversions<actor_control_block*, 0> {
  using type = strong_actor_ptr;
};

template <class T>
struct implicit_conversions<T, error_type_flag> {
  using type = error;
};

template <class T>
struct implicit_conversions<T*, 0> {
  static constexpr int oracle = implicit_conversion_oracle<T>::value;
  static constexpr int is_actor_mask = dynamically_typed_actor_flag
                                       | statically_typed_actor_flag;
  static_assert((oracle & is_actor_mask) != 0,
                "messages must not contain pointers");
  using type = typename implicit_conversions<T, oracle>::type;
};

template <>
struct implicit_conversions<char*, 0> {
  using type = std::string;
};

template <size_t N>
struct implicit_conversions<char[N], 0> : implicit_conversions<char*> {};

template <>
struct implicit_conversions<const char*, 0> : implicit_conversions<char*> {};

template <size_t N>
struct implicit_conversions<const char[N], 0> : implicit_conversions<char*> {};

template <>
struct implicit_conversions<char16_t*, 0> {
  using type = std::u16string;
};

template <size_t N>
struct implicit_conversions<char16_t[N], 0> : implicit_conversions<char16_t*> {
};

template <>
struct implicit_conversions<const char16_t*, 0>
  : implicit_conversions<char16_t*> {};

template <size_t N>
struct implicit_conversions<const char16_t[N], 0>
  : implicit_conversions<char16_t*> {};

template <>
struct implicit_conversions<char32_t*, 0> {
  using type = std::u16string;
};

template <size_t N>
struct implicit_conversions<char32_t[N], 0> : implicit_conversions<char32_t*> {
};

template <>
struct implicit_conversions<const char32_t*, 0>
  : implicit_conversions<char32_t*> {};

template <size_t N>
struct implicit_conversions<const char32_t[N], 0>
  : implicit_conversions<char32_t*> {};

template <>
struct implicit_conversions<scoped_actor, 0> {
  using type = actor;
};

template <class T>
using implicit_conversions_t = typename implicit_conversions<T>::type;

template <class T>
struct strip_and_convert {
  using type = typename implicit_conversions<typename std::remove_const<
    typename std::remove_reference<T>::type>::type>::type;
};

template <class T>
using strip_and_convert_t = typename strip_and_convert<T>::type;

} // namespace detail
} // namespace caf
