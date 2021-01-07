// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/delegated.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/none.hpp"
#include "caf/optional.hpp"
#include "caf/response_promise.hpp"
#include "caf/skip.hpp"
#include "caf/static_visitor.hpp"
#include "caf/typed_response_promise.hpp"
#include "caf/unit.hpp"

namespace caf::detail {

template <class T>
struct is_message_id_wrapper {
  template <class U>
  static char (&test(typename U::message_id_wrapper_tag))[1];
  template <class U>
  static char (&test(...))[2];
  static constexpr bool value = sizeof(test<T>(0)) == 1;
};

template <class T>
struct is_response_promise : std::false_type {};

template <>
struct is_response_promise<response_promise> : std::true_type {};

template <class... Ts>
struct is_response_promise<typed_response_promise<Ts...>> : std::true_type {};

template <class... Ts>
struct is_response_promise<delegated<Ts...>> : std::true_type {};

template <class T>
struct optional_message_visitor_enable_tpl {
  static constexpr bool value
    = !is_one_of<typename std::remove_const<T>::type, none_t, unit_t, skip_t,
                 optional<skip_t>>::value
      && !is_message_id_wrapper<T>::value && !is_response_promise<T>::value;
};

class CAF_CORE_EXPORT optional_message_visitor
  : public static_visitor<optional<message>> {
public:
  optional_message_visitor() = default;

  using opt_msg = optional<message>;

  opt_msg operator()(const none_t&) const {
    return none;
  }

  opt_msg operator()(const skip_t&) const {
    return none;
  }

  opt_msg operator()(const unit_t&) const {
    return message{};
  }

  opt_msg operator()(optional<skip_t>& val) const {
    if (val)
      return none;
    return message{};
  }

  opt_msg operator()(opt_msg& msg) {
    return msg;
  }

  template <class T>
  typename std::enable_if<is_response_promise<T>::value, opt_msg>::type
  operator()(const T&) const {
    return message{};
  }

  template <class T, class... Ts>
  typename std::enable_if<optional_message_visitor_enable_tpl<T>::value,
                          opt_msg>::type
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

} // namespace caf::detail
