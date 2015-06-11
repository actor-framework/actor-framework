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

#include "caf/detail/tail_argument_token.hpp"

namespace caf {
namespace detail {

class timeout_definition_builder {
public:
  constexpr timeout_definition_builder(const duration& d) : tout_(d) {
    // nop
  }

  template <class F>
  timeout_definition<F> operator>>(F f) const {
    return {tout_, std::move(f)};
  }

private:
  duration tout_;
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
      : left_(std::move(l)),
        right_(std::move(r)) {
    // nop
  }

  template <class F>
  auto operator>>(F f) const
  -> std::tuple<decltype(*static_cast<Left*>(nullptr) >> f),
                decltype(*static_cast<Right*>(nullptr) >> f)> {
    return std::make_tuple(left_ >> f, right_ >> f);
  }

private:
  Left left_;
  Right right_;
};

struct tuple_maker {
  template <class... Ts>
  inline auto operator()(Ts&&... xs)
    -> decltype(std::make_tuple(std::forward<Ts>(xs)...)) {
    return std::make_tuple(std::forward<Ts>(xs)...);
  }
};

struct variadic_ctor {};

template <class F, class Projections, class Pattern>
struct advanced_match_case_factory {
  static constexpr bool uses_arg_match =
    std::is_same<
      arg_match_t,
      typename detail::tl_back<Pattern>::type
    >::value;

  using fargs = typename get_callable_trait<F>::arg_types;

  static constexpr size_t fargs_size =
    detail::tl_size<
      fargs
    >::value;

  using decayed_fargs =
    typename tl_map<
      fargs,
      std::decay
    >::type;

  using concatenated_pattern =
    typename detail::tl_concat<
      typename detail::tl_pop_back<Pattern>::type,
      decayed_fargs
    >::type;

  using full_pattern =
    typename std::conditional<
      uses_arg_match,
      concatenated_pattern,
      Pattern
    >::type;

  using filtered_pattern =
    typename detail::tl_filter_not_type<
      full_pattern,
      anything
    >::type;

  using padded_projections =
    typename detail::tl_pad_right<
      Projections,
      detail::tl_size<filtered_pattern>::value
    >::type;

  using projection_funs =
    typename detail::tl_apply<
      padded_projections,
      std::tuple
    >::type;

  using tuple_type =
    typename detail::tl_apply<
      typename detail::tl_zip_right<
        typename detail::tl_map<
          padded_projections,
          projection_result
        >::type,
        fargs,
        detail::left_or_right,
        fargs_size
      >::type,
      std::tuple
    >::type;

  using padding =
    typename detail::tl_apply<
      typename detail::tl_replicate<fargs_size, unit_t>::type,
      std::tuple
    >::type;

  static projection_funs pad(projection_funs res, std::false_type) {
    return std::move(res);
  }

  template <class Guards>
  static projection_funs pad(Guards gs, std::true_type) {
    return std::tuple_cat(std::move(gs), padding{});
  }

  using case_type = advanced_match_case_impl<F, tuple_type,
                                             full_pattern, projection_funs>;

  template <class Guards>
  static case_type create(F f, Guards gs) {
    std::integral_constant<bool, uses_arg_match> tk;
    return {tl_exists<full_pattern, is_anything>::value,
            make_type_token_from_list<full_pattern>(), std::move(f),
            pad(std::move(gs), tk)};
  }
};

template <class X, class Y>
struct pattern_projection_zipper {
  using type = Y;
};

template <class Y>
struct pattern_projection_zipper<anything, Y> {
  using type = none_t;
};

template <class Projections, class Pattern>
class advanced_match_case_builder : public message_case_builder {
public:
  using filtered_projections =
    typename tl_filter_not_type<
      typename tl_zip<
        Pattern,
        Projections,
        pattern_projection_zipper
      >::type,
      none_t
    >::type;

  using guards_tuple =
      typename detail::tl_apply<
        typename std::conditional<
          std::is_same<
            arg_match_t,
            typename tl_back<Pattern>::type
          >::value,
          typename tl_pop_back<filtered_projections>::type,
          filtered_projections
        >::type,
        std::tuple
      >::type;

  template <class... Fs>
  advanced_match_case_builder(variadic_ctor, Fs... fs)
      : g_(make_guards(Pattern{}, fs...)) {
    // nop
  }

  template <class F>
  typename advanced_match_case_factory<F, Projections, Pattern>::case_type
  operator>>(F f) const {
    return advanced_match_case_factory<F, Projections, Pattern>::create(f, g_);
  }

private:
  template <class... Fs>
  static guards_tuple make_guards(tail_argument_token&, Fs&... fs) {
    return std::make_tuple(std::move(fs)...);
  }

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

  guards_tuple g_;
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
