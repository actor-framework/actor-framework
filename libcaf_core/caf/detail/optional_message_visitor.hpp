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

#include "caf/none.hpp"
#include "caf/unit.hpp"
#include "caf/optional.hpp"
#include "caf/delegated.hpp"
#include "caf/skip.hpp"
#include "caf/static_visitor.hpp"
#include "caf/response_promise.hpp"
#include "caf/typed_response_promise.hpp"

#include "caf/detail/type_traits.hpp"

namespace caf {
namespace detail {

template <class T>
struct is_message_id_wrapper {
  template <class U>
  static char (&test(typename U::message_id_wrapper_tag))[1];
  template <class U>
  static char (&test(...))[2];
  static constexpr bool value = sizeof(test<T>(0)) == 1;
};

template <class T>
struct is_response_promise : std::false_type { };

template <>
struct is_response_promise<response_promise> : std::true_type { };

template <class... Ts>
struct is_response_promise<typed_response_promise<Ts...>> : std::true_type { };

template <class... Ts>
struct is_response_promise<delegated<Ts...>> : std::true_type { };


template <class T>
struct optional_message_visitor_enable_tpl {
  static constexpr bool value =
      !is_one_of<
        typename std::remove_const<T>::type,
        none_t,
        unit_t,
        skip_t,
        optional<skip_t>
      >::value
      && !is_message_id_wrapper<T>::value
      && !is_response_promise<T>::value;
};

class optional_message_visitor : public static_visitor<optional<message>> {
public:
  optional_message_visitor() = default;

  using opt_msg = optional<message>;

  inline opt_msg operator()(const none_t&) const {
    return none;
  }

  inline opt_msg operator()(const skip_t&) const {
    return none;
  }

  inline opt_msg operator()(const unit_t&) const {
    return message{};
  }

  inline opt_msg operator()(optional<skip_t>& val) const {
    if (val)
      return none;
    return message{};
  }

  inline opt_msg operator()(opt_msg& msg) {
    return msg;
  }

  template <class T>
  typename std::enable_if<is_response_promise<T>::value, opt_msg>::type
  operator()(const T&) const {
    return message{};
  }

  template <class T, class... Ts>
  typename std::enable_if<
    optional_message_visitor_enable_tpl<T>::value,
    opt_msg
  >::type
  operator()(T& x, Ts&... xs) const {
    return make_message(std::move(x), std::move(xs)...);
  }

  template <class T>
  typename std::enable_if<is_message_id_wrapper<T>::value, opt_msg>::type
  operator()(T& value) const {
    return make_message(atom("MESSAGE_ID"),
                        value.get_message_id().integer_value());
  }

  template <class... Ts>
  opt_msg operator()(std::tuple<Ts...>& value) const {
    return apply_args(*this, get_indices(value), value);
  }

  template <class T>
  opt_msg operator()(optional<T>& value) const {
    if (value)
      return (*this)(*value);
    if (value.empty())
      return message{};
    return value.error();
  }
};

} // namespace detail
} // namespace caf

