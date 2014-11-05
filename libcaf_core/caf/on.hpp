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

#ifndef CAF_ON_HPP
#define CAF_ON_HPP

#include <chrono>
#include <memory>
#include <functional>
#include <type_traits>

#include "caf/unit.hpp"
#include "caf/atom.hpp"
#include "caf/anything.hpp"
#include "caf/message.hpp"
#include "caf/duration.hpp"
#include "caf/match_expr.hpp"
#include "caf/skip_message.hpp"
#include "caf/may_have_timeout.hpp"
#include "caf/timeout_definition.hpp"

#include "caf/detail/type_list.hpp"
#include "caf/detail/arg_match_t.hpp"
#include "caf/detail/type_traits.hpp"

#include "caf/detail/boxed.hpp"
#include "caf/detail/unboxed.hpp"
#include "caf/detail/implicit_conversions.hpp"

namespace caf {
namespace detail {

template <bool IsFun, typename T>
struct add_ptr_to_fun_ {
  using type = T*;

};

template <class T>
struct add_ptr_to_fun_<false, T> {
  using type = T;

};

template <class T>
struct add_ptr_to_fun : add_ptr_to_fun_<std::is_function<T>::value, T> {};

template <bool ToVoid, typename T>
struct to_void_impl {
  using type = unit_t;

};

template <class T>
struct to_void_impl<false, T> {
  using type = typename add_ptr_to_fun<T>::type;

};

template <class T>
struct boxed_to_void : to_void_impl<is_boxed<T>::value, T> {};

template <class T>
struct boxed_to_void<std::function<
  optional<T>(const T&)>> : to_void_impl<is_boxed<T>::value, T> {};

template <class T>
struct boxed_and_callable_to_void
  : to_void_impl<is_boxed<T>::value || detail::is_callable<T>::value, T> {};

class behavior_rvalue_builder {

 public:

  constexpr behavior_rvalue_builder(const duration& d) : m_tout(d) {}

  template <class F>
  timeout_definition<F> operator>>(F&& f) const {
    return {m_tout, std::forward<F>(f)};
  }

 private:

  duration m_tout;

};

struct rvalue_builder_args_ctor {};

template <class Left, class Right>
struct disjunct_rvalue_builders {

 public:

  disjunct_rvalue_builders(Left l, Right r)
      : m_left(std::move(l)), m_right(std::move(r)) {}

  template <class Expr>
  auto operator>>(Expr expr)
    -> decltype((*(static_cast<Left*>(nullptr)) >> expr)
            .or_else(*(static_cast<Right*>(nullptr)) >>
                 expr)) const {
    return (m_left >> expr).or_else(m_right >> expr);
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

template <class Transformers, class Pattern>
struct rvalue_builder {

  using back_type = typename detail::tl_back<Pattern>::type;

  static constexpr bool is_complete =
    !std::is_same<detail::arg_match_t, back_type>::value;

  using fun_container =
    typename detail::tl_apply<
      Transformers,
      std::tuple
    >::type;

  fun_container m_funs;

 public:

  rvalue_builder() = default;

  template <class... Ts>
  rvalue_builder(rvalue_builder_args_ctor, const Ts&... args)
      : m_funs(args...) {}

  rvalue_builder(fun_container arg1) : m_funs(std::move(arg1)) {}

  template <class Expr>
  match_expr<
    typename get_case<is_complete, Expr, Transformers, Pattern>::type>
  operator>>(Expr expr) const {
    using lifted_expr =
      typename get_case<
        is_complete,
        Expr,
        Transformers,
        Pattern
      >::type;
    // adjust m_funs to exactly match expected projections in match case
    using target = typename lifted_expr::projections_list;
    using trimmed_projections = typename tl_trim<Transformers>::type;
    tuple_maker f;
    auto lhs = apply_args(f, get_indices(trimmed_projections{}), m_funs);
    typename tl_apply<
      typename tl_slice<target, tl_size<trimmed_projections>::value,
                tl_size<target>::value>::type,
      std::tuple>::type rhs;
    // done
    return lifted_expr{std::move(expr), std::tuple_cat(lhs, rhs)};
  }

  template <class T, class P>
  disjunct_rvalue_builders<rvalue_builder, rvalue_builder<T, P>>
  operator||(rvalue_builder<T, P> other) const {
    return {*this, std::move(other)};
  }

};

template <bool IsCallable, typename T>
struct pattern_type_ {
  using ctrait = detail::get_callable_trait<T>;
  using args = typename ctrait::arg_types;
  static_assert(detail::tl_size<args>::value == 1,
          "only unary functions allowed");
  using type =
    typename std::decay<
      typename detail::tl_head<args>::type
    >::type;
};

template <class T>
struct pattern_type_<false, T> {
  using type =
    typename implicit_conversions<
      typename std::decay<
        typename detail::unboxed<T>::type
      >::type
    >::type;
};

template <class T>
struct pattern_type
  : pattern_type_<
      detail::is_callable<T>::value && !detail::is_boxed<T>::value, T> { };

} // namespace detail
} // namespace caf

namespace caf {

/**
 * A wildcard that matches any number of any values.
 */
constexpr anything any_vals = anything{};

#ifdef CAF_DOCUMENTATION

/**
 * A wildcard that matches the argument types
 * of a given callback. Must be the last argument to {@link on()}.
 * @see {@link math_actor_example.cpp Math Actor Example}
 */
constexpr __unspecified__ arg_match;

/**
 * Left-hand side of a partial function expression.
 *
 * Equal to `on(arg_match).
 */
constexpr __unspecified__ on_arg_match;

/**
 * A wildcard that matches any value of type `T`.
 * @see {@link math_actor_example.cpp Math Actor Example}
 */
template <class T>
__unspecified__ val();

/**
 * Left-hand side of a partial function expression that matches values.
 *
 * This overload can be used with the wildcards {@link caf::val val},
 * {@link caf::any_vals any_vals} and {@link caf::arg_match arg_match}.
 */
template <class T, class... Ts>
__unspecified__ on(const T& arg, const Ts&... args);

/**
 * Left-hand side of a partial function expression that matches types.
 *
 * This overload matches types only. The type {@link caf::anything anything}
 * can be used as wildcard to match any number of elements of any types.
 */
template <class... Ts>
__unspecified__ on();

/**
 * Left-hand side of a partial function expression that matches types.
 *
 * This overload matches up to four leading atoms.
 * The type {@link caf::anything anything}
 * can be used as wildcard to match any number of elements of any types.
 */
template <atom_value... Atoms, class... Ts>
__unspecified__ on();

/**
 * Converts `arg` to a match expression by returning
 *    `on_arg_match >> arg if `arg` is a callable type,
 *    otherwise returns `arg`.
 */
template <class T>
__unspecified__ lift_to_match_expr(T arg);

#else

template <class T>
constexpr typename detail::boxed<T>::type val() {
  return typename detail::boxed<T>::type();
}

using boxed_arg_match_t = typename detail::boxed<detail::arg_match_t>::type;

constexpr boxed_arg_match_t arg_match = boxed_arg_match_t();

template <class T, typename Predicate>
std::function<optional<T>(const T&)> guarded(Predicate p, T value) {
  return[=](const T & other)->optional<T> {
    if (p(other, value)) return value;
  return none;

};
}

// special case covering arg_match as argument to guarded()
template <class T, typename Predicate>
unit_t guarded(Predicate, const detail::wrapped<T>&) {
  return unit;
}

inline unit_t to_guard(const anything&) {
  return unit;
}

template <class T>
unit_t to_guard(detail::wrapped<T> (*)()) {
  return unit;
}

template <class T>
unit_t to_guard(const detail::wrapped<T>&) {
  return unit;
}

template <class T>
std::function<optional<typename detail::strip_and_convert<T>::type>(
  const typename detail::strip_and_convert<T>::type&)>
to_guard(const T& value,
         typename std::enable_if<!detail::is_callable<T>::value>::type* = 0) {
  using type = typename detail::strip_and_convert<T>::type;
  return guarded<type>(std::equal_to<type>{}, value);
}

template <class F>
F to_guard(F fun,
           typename std::enable_if<detail::is_callable<F>::value>::type* = 0) {
  return fun;
}

template <class T, class... Ts>
auto on(const T& arg, const Ts&... args)
-> detail::rvalue_builder<
    detail::type_list<
      decltype(to_guard(arg)),
      decltype(to_guard(args))...
    >,
    detail::type_list<
      typename detail::pattern_type<T>::type,
      typename detail::pattern_type<Ts>::type...>
    > {
  return {detail::rvalue_builder_args_ctor{}, to_guard(arg), to_guard(args)...};
}

inline detail::rvalue_builder<detail::empty_type_list, detail::empty_type_list>
on() {
  return {};
}

template <class T0, class... Ts>
detail::rvalue_builder<detail::empty_type_list, detail::type_list<T0, Ts...>>
on() {
  return {};
}

template <atom_value A0, class... Ts>
decltype(on(A0, val<Ts>()...)) on() {
  return on(A0, val<Ts>()...);
}

template <atom_value A0, atom_value A1, class... Ts>
decltype(on(A0, A1, val<Ts>()...)) on() {
  return on(A0, A1, val<Ts>()...);
}

template <atom_value A0, atom_value A1, atom_value A2, class... Ts>
decltype(on(A0, A1, A2, val<Ts>()...)) on() {
  return on(A0, A1, A2, val<Ts>()...);
}

template <atom_value A0, atom_value A1, atom_value A2, atom_value A3,
          class... Ts>
decltype(on(A0, A1, A2, A3, val<Ts>()...)) on() {
  return on(A0, A1, A2, A3, val<Ts>()...);
}

template <class Rep, class Period>
constexpr detail::behavior_rvalue_builder
after(const std::chrono::duration<Rep, Period>& d) {
  return {duration(d)};
}

inline decltype(on<anything>()) others() { return on<anything>(); }

// some more convenience

namespace detail {

class on_the_fly_rvalue_builder {

 public:

  constexpr on_the_fly_rvalue_builder() {}

  template <class Expr>
  match_expr<typename get_case<false, Expr, detail::empty_type_list,
                 detail::empty_type_list>::type>
  operator>>(Expr expr) const {
    using result_type =
      typename get_case<
        false,
        Expr,
        detail::empty_type_list,
        detail::empty_type_list
      >::type;
    return result_type{std::move(expr)};
  }

};

} // namespace detail

constexpr detail::on_the_fly_rvalue_builder on_arg_match;

#endif // CAF_DOCUMENTATION

} // namespace caf

#endif // CAF_ON_HPP
