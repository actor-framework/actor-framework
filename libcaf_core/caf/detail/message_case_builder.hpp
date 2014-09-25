/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#ifndef CAF_DETAIL_MESSAGE_CASE_BUILDER_HPP
#define CAF_DETAIL_MESSAGE_CASE_BUILDER_HPP

#include <type_traits>

#include "caf/duration.hpp"
#include "caf/match_case.hpp"
#include "caf/timeout_definition.hpp"

namespace caf {
namespace detail {

class timeout_definition_builder {
 public:
  constexpr timeout_definition_builder(const duration& d) : m_tout(d) {
    // nop
  }

  template <class F>
  timeout_definition<F> operator>>(F f) const {
    return {m_tout, std::move(f)};
  }

 private:
  duration m_tout;
};

class message_case_builder { };

class trivial_match_case_builder : public message_case_builder {
 public:
  constexpr trivial_match_case_builder() {
    // nop
  }

  template <class F>
  trivial_match_case<F> operator>>(F f) const {
    return {std::move(f)};
  }
};

class catch_all_match_case_builder : public message_case_builder {
 public:
  constexpr catch_all_match_case_builder() {
    // nop
  }

  const catch_all_match_case_builder& operator()() const {
    return *this;
  }

  template <class F>
  catch_all_match_case<F> operator>>(F f) const {
    return {std::move(f)};
  }
};

template <class Left, class Right>
class message_case_pair_builder : public message_case_builder {
 public:
  message_case_pair_builder(Left l, Right r)
      : m_left(std::move(l)),
        m_right(std::move(r)) {
    // nop
  }

  template <class F>
  auto operator>>(F f) const
  -> std::tuple<decltype(*static_cast<Left*>(nullptr) >> f),
                decltype(*static_cast<Right*>(nullptr) >> f)> {
    return std::make_tuple(m_left >> f, m_right >> f);
  }

 private:
  Left m_left;
  Right m_right;
};

struct tuple_maker {
  template <class... Ts>
  inline auto operator()(Ts&&... args)
    -> decltype(std::make_tuple(std::forward<Ts>(args)...)) {
    return std::make_tuple(std::forward<Ts>(args)...);
  }
};

struct variadic_ctor {};

template <class Expr, class Transformers, class Pattern>
struct get_advanced_match_case {
  using ctrait = typename get_callable_trait<Expr>::type;

  using filtered_pattern =
    typename tl_filter_not_type<
      Pattern,
      anything
    >::type;

  using padded_transformers =
    typename tl_pad_right<
      Transformers,
      tl_size<filtered_pattern>::value
    >::type;

  using base_signature =
    typename tl_map<
      filtered_pattern,
      std::add_const,
      std::add_lvalue_reference
    >::type;

  using padded_expr_args =
    typename tl_map_conditional<
      typename tl_pad_left<
        typename ctrait::arg_types,
        tl_size<filtered_pattern>::value
      >::type,
      std::is_lvalue_reference,
      false,
      std::add_const,
      std::add_lvalue_reference
    >::type;

  // override base signature with required argument types of Expr
  // and result types of transformation
  using partial_fun_signature =
    typename tl_zip<
      typename tl_map<
        padded_transformers,
        map_to_result_type,
        rm_optional,
        std::add_lvalue_reference
      >::type,
      typename tl_zip<
        padded_expr_args,
        base_signature,
        left_or_right
      >::type,
      left_or_right
    >::type;

  // 'inherit' mutable references from partial_fun_signature
  // for arguments without transformation
  using projection_signature =
    typename tl_zip<
      typename tl_zip<
        padded_transformers,
        partial_fun_signature,
        if_not_left
      >::type,
      base_signature,
      deduce_ref_type
    >::type;

  using type =
    advanced_match_case_impl<
      Expr,
      Pattern,
      padded_transformers,
      projection_signature
    >;
};

template <class X, class Y>
struct pattern_projection_zipper {
  using type = Y;
};

template <class Y>
struct pattern_projection_zipper<anything, Y> {
  using type = none_t;
};

template <class Projections, class Pattern, bool UsesArgMatch>
class advanced_match_case_builder : public message_case_builder {
 public:
  using guards_tuple =
    typename tl_apply<
      // discard projection if at the same index in Pattern is a wildcard
      typename tl_filter_not_type<
        typename tl_zip<Pattern, Projections, pattern_projection_zipper>::type,
        none_t
      >::type,
      std::tuple
    >::type;

  advanced_match_case_builder() = default;

  template <class... Fs>
  advanced_match_case_builder(variadic_ctor, Fs... fs)
      : m_guards(make_guards(Pattern{}, fs...)) {
    // nop
  }

  advanced_match_case_builder(guards_tuple arg1) : m_guards(std::move(arg1)) {
    // nop
  }

  template <class F>
  typename get_advanced_match_case<F, Projections, Pattern>::type
  operator>>(F f) const {
    return {f, m_guards};
  }

 private:
  template <class... Fs>
  static guards_tuple make_guards(tail_argument_token&, Fs&... fs) {
    return std::make_tuple(std::move(fs)...);
  }

  template <class T, class F, class... Fs>
  static guards_tuple make_guards(std::pair<wrapped<T>, F> x, Fs&&... fs) {
    return make_guards(std::forward<Fs>(fs)..., x.second);
  }

  template <class F, class... Fs>
  static guards_tuple make_guards(std::pair<wrapped<anything>, F>, Fs&&... fs) {
    return make_guards(std::forward<Fs>(fs)...);
  }

  template <class... Ts, class... Fs>
  static guards_tuple make_guards(type_list<Ts...>, Fs&... fs) {
    tail_argument_token eoa;
    return make_guards(std::make_pair(wrapped<Ts>(), std::ref(fs))..., eoa);
  }

  guards_tuple m_guards;
};

template <class Projections, class Pattern>
struct advanced_match_case_builder<Projections, Pattern, true>
    : public message_case_builder {
 public:
  using guards_tuple =
    typename detail::tl_apply<
      typename tl_pop_back<Projections>::type,
      std::tuple
    >::type;

  using pattern = typename tl_pop_back<Pattern>::type;

  advanced_match_case_builder() = default;

  template <class... Fs>
  advanced_match_case_builder(variadic_ctor, Fs... fs)
      : m_guards(make_guards(Pattern{}, fs...)) {
    // nop
  }

  advanced_match_case_builder(guards_tuple arg1) : m_guards(std::move(arg1)) {
    // nop
  }

  template <class F>
  typename get_advanced_match_case<
    F,
    Projections,
    typename tl_concat<
      pattern,
      typename tl_map<
        typename get_callable_trait<F>::arg_types,
        std::decay
      >::type
    >::type
  >::type
  operator>>(F f) const {
    using padding =
      typename tl_apply<
        typename tl_replicate<get_callable_trait<F>::num_args, unit_t>::type,
        std::tuple
      >::type;
    return {f, std::tuple_cat(m_guards, padding{})};
  }

 private:
  static guards_tuple make_guards(tail_argument_token&) {
    return {};
  }

  template <class F, class... Fs>
  static guards_tuple make_guards(std::pair<wrapped<arg_match_t>, F>,
                                 tail_argument_token&, Fs&... fs) {
    return std::make_tuple(std::move(fs)...);
  }

  template <class T, class F, class... Fs>
  static guards_tuple make_guards(std::pair<wrapped<T>, F> x, Fs&&... fs) {
    return make_guards(std::forward<Fs>(fs)..., x.second);
  }

  template <class F, class... Fs>
  static guards_tuple make_guards(std::pair<wrapped<anything>, F>, Fs&&... fs) {
    return make_guards(std::forward<Fs>(fs)...);
  }

  template <class... Ts, class... Fs>
  static guards_tuple make_guards(type_list<Ts...>, Fs&... fs) {
    tail_argument_token eoa;
    return make_guards(std::make_pair(wrapped<Ts>(), std::ref(fs))..., eoa);
  }

  guards_tuple m_guards;
};

template <class Left, class Right>
typename std::enable_if<
  std::is_base_of<message_case_builder, Left>::value
  && std::is_base_of<message_case_builder, Right>::value,
  message_case_pair_builder<Left, Right>
>::type
operator||(Left l, Right r) {
  return {l, r};
}

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_MESSAGE_CASE_BUILDER_HPP
