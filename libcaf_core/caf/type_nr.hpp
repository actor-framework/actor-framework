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

#include <map>
#include <set>
#include <string>
#include <vector>
#include <cstdint>

#include "caf/atom.hpp"
#include "caf/fwd.hpp"
#include "caf/timespan.hpp"
#include "caf/timestamp.hpp"

#include "caf/detail/type_list.hpp"
#include "caf/detail/squashed_int.hpp"

namespace caf {

/// Compile-time list of all built-in types.
/// @warning Types are sorted by uniform name.
using sorted_builtin_types =
  detail::type_list<
    actor,                              // @actor
    std::vector<actor>,                 // @actorvec
    actor_addr,                         // @addr
    std::vector<actor_addr>,            // @addrvec
    atom_value,                         // @atom
    std::vector<char>,                  // @charbuf
    down_msg,                           // @down
    downstream_msg,                     // @downstream_msg
    duration,                           // @duration
    error,                              // @error
    exit_msg,                           // @exit
    group,                              // @group
    group_down_msg,                     // @group_down
    int16_t,                            // @i16
    int32_t,                            // @i32
    int64_t,                            // @i64
    int8_t,                             // @i8
    long double,                        // @ldouble
    message,                            // @message
    message_id,                         // @message_id
    node_id,                            // @node
    open_stream_msg,                    // @open_stream_msg
    std::string,                        // @str
    std::map<std::string, std::string>, // @strmap
    strong_actor_ptr,                   // @strong_actor_ptr
    std::set<std::string>,              // @strset
    std::vector<std::string>,           // @strvec
    timeout_msg,                        // @timeout
    timespan,                           // @timespan
    timestamp,                          // @timestamp
    uint16_t,                           // @u16
    std::u16string,                     // @u16_str
    uint32_t,                           // @u32
    std::u32string,                     // @u32_str
    uint64_t,                           // @u64
    uint8_t,                            // @u8
    unit_t,                             // @unit
    upstream_msg,                       // @upstream_msg
    weak_actor_ptr,                     // @weak_actor_ptr
    bool,                               // bool
    double,                             // double
    float                               // float
  >;

/// Computes the type number for `T`.
template <class T, bool IsIntegral = std::is_integral<T>::value>
struct type_nr {
  static constexpr uint16_t value = static_cast<uint16_t>(
    detail::tl_index_of<sorted_builtin_types, T>::value + 1);
};

template <class T>
struct type_nr<T, true> {
  using type = detail::squashed_int_t<T>;
  static constexpr uint16_t value = static_cast<uint16_t>(
    detail::tl_index_of<sorted_builtin_types, type>::value + 1);
};

template <>
struct type_nr<bool, true> {
  static constexpr uint16_t value = static_cast<uint16_t>(
    detail::tl_index_of<sorted_builtin_types, bool>::value + 1);
};

template <atom_value V>
struct type_nr<atom_constant<V>, false> {
  static constexpr uint16_t value = type_nr<atom_value>::value;
};

/// The number of built-in types.
static constexpr size_t type_nrs = detail::tl_size<sorted_builtin_types>::value
                                   + 1;

/// List of all type names, indexed via `type_nr`.
extern const char* numbered_type_names[];

template <uint32_t R, uint16_t... Is>
struct type_token_helper;

template <uint32_t R>
struct type_token_helper<R> : std::integral_constant<uint32_t, R> {
  // nop
};

template <uint32_t R, uint16_t I, uint16_t... Is>
struct type_token_helper<R, I, Is...> : type_token_helper<(R << 6) | I, Is...> {
  // nop
};

template <class... Ts>
constexpr uint32_t make_type_token() {
  return type_token_helper<0xFFFFFFFF, type_nr<Ts>::value...>::value;
}

constexpr uint32_t add_to_type_token(uint32_t token, uint16_t tnr) {
  return (token << 6) | tnr;
}

template <class T>
struct make_type_token_from_list_helper;

template <class... Ts>
struct make_type_token_from_list_helper<detail::type_list<Ts...>>
    : type_token_helper<0xFFFFFFFF, type_nr<Ts>::value...> {
  // nop
};

template <class T>
constexpr uint32_t make_type_token_from_list() {
  return make_type_token_from_list_helper<T>::value;
}

} // namespace caf

