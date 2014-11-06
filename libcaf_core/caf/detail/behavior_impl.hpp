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

#ifndef CAF_DETAIL_BEHAVIOR_IMPL_HPP
#define CAF_DETAIL_BEHAVIOR_IMPL_HPP

#include <tuple>
#include <type_traits>

#include "caf/none.hpp"
#include "caf/variant.hpp"
#include "caf/optional.hpp"
#include "caf/intrusive_ptr.hpp"

#include "caf/atom.hpp"
#include "caf/message.hpp"
#include "caf/duration.hpp"
#include "caf/ref_counted.hpp"
#include "caf/skip_message.hpp"
#include "caf/timeout_definition.hpp"

#include "caf/detail/int_list.hpp"
#include "caf/detail/apply_args.hpp"
#include "caf/detail/type_traits.hpp"

// <backward_compatibility version="0.9">
#include "cppa/cow_tuple.hpp"
// </backward_compatibility>

namespace caf {

class message_handler;
using bhvr_invoke_result = optional<message>;

} // namespace caf

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
struct optional_message_visitor_enable_tpl {
  static constexpr bool value =
      !detail::is_one_of<
        typename std::remove_const<T>::type,
        none_t,
        unit_t,
        skip_message_t,
        optional<skip_message_t>
      >::value
      && !is_message_id_wrapper<T>::value;
};

struct optional_message_visitor : static_visitor<bhvr_invoke_result> {

  inline bhvr_invoke_result operator()(const none_t&) const {
    return none;
  }

  inline bhvr_invoke_result operator()(const skip_message_t&) const {
    return none;
  }

  inline bhvr_invoke_result operator()(const unit_t&) const {
    return message{};
  }

  inline bhvr_invoke_result operator()(const optional<skip_message_t>& val) const {
    if (val) return none;
    return message{};
  }

  template <class T, class... Ts>
  typename std::enable_if<optional_message_visitor_enable_tpl<T>::value,
              bhvr_invoke_result>::type
  operator()(T& v, Ts&... vs) const {
    return make_message(std::move(v), std::move(vs)...);
  }

  template <class T>
  typename std::enable_if<is_message_id_wrapper<T>::value,
              bhvr_invoke_result>::type
  operator()(T& value) const {
    return make_message(atom("MESSAGE_ID"),
              value.get_message_id().integer_value());
  }

  inline bhvr_invoke_result operator()(message& value) const {
    return std::move(value);
  }

  template <class... Ts>
  bhvr_invoke_result operator()(std::tuple<Ts...>& value) const {
    return detail::apply_args(*this, detail::get_indices(value), value);
  }

  // <backward_compatibility version="0.9">
  template <class... Ts>
  bhvr_invoke_result operator()(cow_tuple<Ts...>& value) const {
    return static_cast<message>(std::move(value));
  }
  // </backward_compatibility>

};

template <class... Ts>
struct has_skip_message {
  static constexpr bool value =
    detail::disjunction<std::is_same<Ts, skip_message_t>::value...>::value;
};

class behavior_impl : public ref_counted {
 public:
  using pointer = intrusive_ptr<behavior_impl>;

  ~behavior_impl();
  behavior_impl() = default;
  behavior_impl(duration tout);

  virtual bhvr_invoke_result invoke(message&) = 0;

  virtual bhvr_invoke_result invoke(const message&) = 0;

  inline bhvr_invoke_result invoke(message&& arg) {
    message tmp(std::move(arg));
    return invoke(tmp);
  }

  virtual void handle_timeout();

  inline const duration& timeout() const {
    return m_timeout;
  }

  virtual pointer copy(const generic_timeout_definition& tdef) const = 0;

  pointer or_else(const pointer& other);

 private:
  duration m_timeout;
};

struct dummy_match_expr {
  inline variant<none_t> invoke(const message&) const {
    return none;
  }
  inline bool can_invoke(const message&) const {
    return false;
  }
  inline variant<none_t> operator()(const message&) const {
    return none;
  }
};

template <class MatchExpr, typename F>
class default_behavior_impl : public behavior_impl {
 public:
  template <class Expr>
  default_behavior_impl(Expr&& expr, const timeout_definition<F>& d)
      : behavior_impl(d.timeout)
      , m_expr(std::forward<Expr>(expr))
      , m_fun(d.handler) {}

  template <class Expr>
  default_behavior_impl(Expr&& expr, duration tout, F f)
      : behavior_impl(tout),
        m_expr(std::forward<Expr>(expr)),
        m_fun(f) {
    // nop
  }

  bhvr_invoke_result invoke(message& tup) {
    auto res = m_expr(tup);
    optional_message_visitor omv;
    return apply_visitor(omv, res);
  }

  bhvr_invoke_result invoke(const message& tup) {
    auto res = m_expr(tup);
    optional_message_visitor omv;
    return apply_visitor(omv, res);
  }

  typename behavior_impl::pointer
  copy(const generic_timeout_definition& tdef) const {
    return new default_behavior_impl<MatchExpr, std::function<void()>>(m_expr,
                                                                       tdef);
  }

  void handle_timeout() {
    m_fun();
  }

 private:
  MatchExpr m_expr;
  F m_fun;
};

template <class MatchExpr, typename F>
default_behavior_impl<MatchExpr, F>*
new_default_behavior(const MatchExpr& mexpr, duration d, F f) {
  return new default_behavior_impl<MatchExpr, F>(mexpr, d, f);
}

template <class F>
behavior_impl* new_default_behavior(duration d, F f) {
  dummy_match_expr nop;
  return new default_behavior_impl<dummy_match_expr, F>(nop, d, std::move(f));
}

behavior_impl* new_default_behavior(duration d, std::function<void()> fun);

using behavior_impl_ptr = intrusive_ptr<behavior_impl>;

// implemented in message_handler.cpp
// message_handler combine(behavior_impl_ptr, behavior_impl_ptr);
// behavior_impl_ptr extract(const message_handler&);

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_BEHAVIOR_IMPL_HPP
