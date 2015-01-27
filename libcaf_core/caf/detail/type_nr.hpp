/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_DETAIL_TYPE_NR_HPP
#define CAF_DETAIL_TYPE_NR_HPP

#include <map>
#include <set>
#include <string>
#include <vector>
#include <cstdint>

#include "caf/fwd.hpp"

#include "caf/detail/type_list.hpp"

namespace caf {
namespace detail {

#define CAF_DETAIL_TYPE_NR(number, tname)                                      \
  template <>                                                                  \
  struct type_nr<tname> {                                                      \
    static constexpr uint16_t value = number;                                  \
  };

template <class T,
          bool IntegralConstant = std::is_integral<T>::value,
          size_t = sizeof(T)>
struct type_nr_helper {
  static constexpr uint16_t value = 0;
};

template <class T>
struct type_nr {
  static constexpr uint16_t value = type_nr_helper<T>::value;
};

template <>
struct type_nr<void> {
  static constexpr uint16_t value = 0;
};

using strmap = std::map<std::string, std::string>;

// WARNING: types are sorted by uniform name
CAF_DETAIL_TYPE_NR( 1, actor)                    // @actor
CAF_DETAIL_TYPE_NR( 2, actor_addr)               // @addr
CAF_DETAIL_TYPE_NR( 3, atom_value)               // @atom
CAF_DETAIL_TYPE_NR( 4, channel)                  // @channel
CAF_DETAIL_TYPE_NR( 5, std::vector<char>)        // @charbuf
CAF_DETAIL_TYPE_NR( 6, down_msg)                 // @down
CAF_DETAIL_TYPE_NR( 7, duration)                 // @duration
CAF_DETAIL_TYPE_NR( 8, exit_msg)                 // @exit
CAF_DETAIL_TYPE_NR( 9, group)                    // @group
CAF_DETAIL_TYPE_NR(10, group_down_msg)           // @group_down
CAF_DETAIL_TYPE_NR(11, int16_t)                  // @i16
CAF_DETAIL_TYPE_NR(12, int32_t)                  // @i32
CAF_DETAIL_TYPE_NR(13, int64_t)                  // @i64
CAF_DETAIL_TYPE_NR(14, int8_t)                   // @i8
CAF_DETAIL_TYPE_NR(15, long double)              // @ldouble
CAF_DETAIL_TYPE_NR(16, message)                  // @message
CAF_DETAIL_TYPE_NR(17, message_id)               // @message_id
CAF_DETAIL_TYPE_NR(18, node_id)                  // @node
CAF_DETAIL_TYPE_NR(19, std::string)              // @str
CAF_DETAIL_TYPE_NR(20, strmap)                   // @strmap
CAF_DETAIL_TYPE_NR(21, std::set<std::string>)    // @strset
CAF_DETAIL_TYPE_NR(22, std::vector<std::string>) // @strvec
CAF_DETAIL_TYPE_NR(23, sync_exited_msg)          // @sync_exited
CAF_DETAIL_TYPE_NR(24, sync_timeout_msg)         // @sync_timeout
CAF_DETAIL_TYPE_NR(25, timeout_msg)              // @timeout
CAF_DETAIL_TYPE_NR(26, uint16_t)                 // @u16
CAF_DETAIL_TYPE_NR(27, std::u16string)           // @u16_str
CAF_DETAIL_TYPE_NR(28, uint32_t)                 // @u32
CAF_DETAIL_TYPE_NR(29, std::u32string)           // @u32_str
CAF_DETAIL_TYPE_NR(30, uint64_t)                 // @u64
CAF_DETAIL_TYPE_NR(31, uint8_t)                  // @u8
CAF_DETAIL_TYPE_NR(32, unit_t)                   // @unit
CAF_DETAIL_TYPE_NR(33, bool)                     // bool
CAF_DETAIL_TYPE_NR(34, double)                   // double
CAF_DETAIL_TYPE_NR(35, float)                    // float

static constexpr size_t type_nrs = 36;

extern const char* numbered_type_names[];

template <class T>
struct type_nr_helper<T, true, 1> {
  static constexpr uint16_t value = std::is_signed<T>::value
                                      ? type_nr<int8_t>::value
                                      : type_nr<uint8_t>::value;
};

template <class T>
struct type_nr_helper<T, true, 2> {
  static constexpr uint16_t value = std::is_signed<T>::value
                                      ? type_nr<int16_t>::value
                                      : type_nr<uint16_t>::value;
};

template <class T>
struct type_nr_helper<T, true, 4> {
  static constexpr uint16_t value = std::is_signed<T>::value
                                      ? type_nr<int32_t>::value
                                      : type_nr<uint32_t>::value;
};

template <class T>
struct type_nr_helper<T, true, 8> {
  static constexpr uint16_t value = std::is_signed<T>::value
                                      ? type_nr<int64_t>::value
                                      : type_nr<uint64_t>::value;
};

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

template <class T>
struct make_type_token_from_list_helper;

template <class... Ts>
struct make_type_token_from_list_helper<type_list<Ts...>>
    : type_token_helper<0xFFFFFFFF, type_nr<Ts>::value...> {
  // nop
};

template <class T>
constexpr uint32_t make_type_token_from_list() {
  return make_type_token_from_list_helper<T>::value;
}

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_TYPE_NR_HPP
