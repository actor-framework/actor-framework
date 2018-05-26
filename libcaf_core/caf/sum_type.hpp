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

#include "caf/detail/type_list.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/sum_type_access.hpp"

namespace caf {

/// Concept for checking whether `T` supports the sum type API by specializing
/// `sum_type_access`.
template <class T>
constexpr bool SumType() {
  return has_sum_type_access<typename std::decay<T>::type>::value;
}

/// Concept for checking whether all `Ts` support the sum type API by
/// specializing `sum_type_access`.
template <class... Ts>
constexpr bool SumTypes() {
  using namespace detail;
  using types = type_list<decay_t<Ts>...>;
  return tl_forall<types, has_sum_type_access>::value;
}

/// Returns a reference to the value of a sum type.
/// @pre `holds_alternative<T>(x)`
/// @relates SumType
template <class T, class U>
detail::enable_if_t<SumType<U>(), T&> get(U& x) {
  using namespace detail;
  using trait = sum_type_access<U>;
  int_token<tl_index_where<typename trait::types,
                           tbind<is_same_ish, T>::template type>::value> token;
  // Silence compiler error about "binding to unrelated types" such as
  // 'signed char' to 'char' (which is obvious bullshit).
  return reinterpret_cast<T&>(trait::get(x, token));
}

/// Returns a reference to the value of a sum type.
/// @pre `holds_alternative<T>(x)`
/// @relates SumType
template <class T, class U>
detail::enable_if_t<SumType<U>(), const T&> get(const U& x) {
  using namespace detail;
  using trait = sum_type_access<U>;
  int_token<tl_index_where<typename trait::types,
                           tbind<is_same_ish, T>::template type>::value> token;
  // Silence compiler error about "binding to unrelated types" such as
  // 'signed char' to 'char' (which is obvious bullshit).
  return reinterpret_cast<const T&>(trait::get(x, token));
}

/// Returns a pointer to the value of a sum type if it is of type `T`,
/// `nullptr` otherwise.
/// @relates SumType
template <class T, class U>
detail::enable_if_t<SumType<U>(), T*> get_if(U* x) {
  using namespace detail;
  using trait = sum_type_access<U>;
  int_token<tl_index_where<typename trait::types,
                           tbind<is_same_ish, T>::template type>::value> token;
  return trait::is(*x, token) ? &trait::get(*x, token) : nullptr;
}

/// Returns a pointer to the value of a sum type if it is of type `T`,
/// `nullptr` otherwise.
/// @relates SumType
template <class T, class U>
detail::enable_if_t<SumType<U>(), const T*> get_if(const U* x) {
  using namespace detail;
  using trait = sum_type_access<U>;
  using token_type =
    int_token<tl_index_where<typename trait::types,
                             tbind<is_same_ish, T>::template type>::value>;
  token_type token;
  static_assert(token_type::value != -1, "T is not part of the sum type");
  return trait::is(*x, token) ? &trait::get(*x, token) : nullptr;
}

/// Returns whether a sum type has a value of type `T`.
/// @relates SumType
template <class T, class U>
bool holds_alternative(const U& x) {
  using namespace detail;
  using trait = sum_type_access<U>;
  int_token<tl_index_where<typename trait::types,
                           tbind<is_same_ish, T>::template type>::value> token;
  return trait::is(x, token);
}

template <bool Valid, class F, class... Ts>
struct sum_type_visit_result_impl {
  using type = decltype((std::declval<F&>())(
    std::declval<typename sum_type_access<Ts>::type0&>()...));
};

template <class F, class... Ts>
struct sum_type_visit_result_impl<false, F, Ts...> {};

template <class F, class... Ts>
struct sum_type_visit_result
    : sum_type_visit_result_impl<
        detail::conjunction<SumType<Ts>()...>::value, F, Ts...> {};

template <class F, class... Ts>
using sum_type_visit_result_t =
  typename sum_type_visit_result<detail::decay_t<F>,
                                 detail::decay_t<Ts>...>::type;

template <class Result, int I, class Visitor>
struct visit_impl_continuation;

template <class Result, int I>
struct visit_impl {
  template <class Visitor, class T, class... Ts>
  static Result apply(Visitor&& f, T&& x, Ts&&... xs) {
    visit_impl_continuation<Result, I - 1, Visitor> continuation{f};
    using trait = sum_type_access<detail::decay_t<T>>;
    return trait::template apply<Result>(x, continuation,
                                         std::forward<Ts>(xs)...);
  }
};

template <class Result>
struct visit_impl<Result, 0> {
  template <class Visitor, class... Ts>
  static Result apply(Visitor&& f, Ts&&... xs) {
    return f(std::forward<Ts>(xs)...);
  }
};


template <class Result, int I, class Visitor>
struct visit_impl_continuation {
  Visitor& f;
  template <class... Ts>
  Result operator()(Ts&&... xs) {
    return visit_impl<Result, I>::apply(f, std::forward<Ts>(xs)...);
  }
};

/// Applies the values of any number of sum types to the visitor.
/// @relates SumType
template <class Visitor, class T, class... Ts,
          class Result = sum_type_visit_result_t<Visitor, T, Ts...>>
detail::enable_if_t<SumTypes<T, Ts...>(), Result>
visit(Visitor&& f, T&& x, Ts&&... xs) {
  return visit_impl<Result, sizeof...(Ts) + 1>::apply(std::forward<Visitor>(f),
                                                      std::forward<T>(x),
                                                      std::forward<Ts>(xs)...);
}


} // namespace caf
