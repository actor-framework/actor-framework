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

#include <tuple>
#include <type_traits>

#include "caf/none.hpp"
#include "caf/param.hpp"
#include "caf/optional.hpp"
#include "caf/match_case.hpp"
#include "caf/skip.hpp"

#include "caf/detail/int_list.hpp"
#include "caf/detail/try_match.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/detail/apply_args.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/detail/pseudo_tuple.hpp"
#include "caf/detail/invoke_result_visitor.hpp"

namespace caf {

class match_case {
public:
  enum result {
    no_match,
    match,
    skip
  };

  match_case(uint32_t tt);

  match_case(match_case&&) = default;
  match_case(const match_case&) = default;

  virtual ~match_case();

  /// Tries to invoke this match case with the contents of `xs`.
  virtual result invoke(detail::invoke_result_visitor& rv,
                        type_erased_tuple& xs) = 0;

  inline uint32_t type_token() const {
    return token_;
  }

private:
  uint32_t token_;
};

template <bool IsVoid, class F>
class lfinvoker {
public:
  lfinvoker(F& fun) : fun_(fun) {
    // nop
  }

  template <class... Ts>
  typename detail::get_callable_trait<F>::result_type operator()(Ts&&... xs) {
    return fun_(std::forward<Ts>(xs)...);
  }

private:
  F& fun_;
};

template <class F>
class lfinvoker<true, F> {
public:
  lfinvoker(F& fun) : fun_(fun) {
    // nop
  }

  template <class... Ts>
  unit_t operator()(Ts&&... xs) {
    fun_(std::forward<Ts>(xs)...);
    return unit;
  }

private:
  F& fun_;
};

template <class F>
class trivial_match_case : public match_case {
public:
  using fun_trait = typename detail::get_callable_trait<F>::type;

  using plain_result_type = typename fun_trait::result_type;

  using result_type =
    typename std::conditional<
      std::is_reference<plain_result_type>::value,
      plain_result_type,
      typename std::remove_const<plain_result_type>::type
    >::type;

  using arg_types = typename fun_trait::arg_types;

  static constexpr bool is_manipulator =
    detail::tl_exists<
      arg_types,
      detail::is_mutable_ref
    >::value;

  using pattern =
    typename detail::tl_map<
      arg_types,
      param_decay
    >::type;

  using decayed_arg_types =
    typename detail::tl_map<
      arg_types,
      std::decay
    >::type;

  using intermediate_pseudo_tuple =
    typename detail::tl_apply<
      decayed_arg_types,
      detail::pseudo_tuple
    >::type;

  trivial_match_case(trivial_match_case&&) = default;
  trivial_match_case(const trivial_match_case&) = default;
  trivial_match_case& operator=(trivial_match_case&&) = default;
  trivial_match_case& operator=(const trivial_match_case&) = default;

  trivial_match_case(F f)
      : match_case(make_type_token_from_list<pattern>()),
        fun_(std::move(f)) {
    // nop
  }

  match_case::result invoke(detail::invoke_result_visitor& f,
                            type_erased_tuple& xs) override {
    detail::meta_elements<pattern> ms;
    // check if try_match() reports success
    if (!detail::try_match(xs, ms.arr.data(), ms.arr.size()))
      return match_case::no_match;
    typename detail::il_indices<decayed_arg_types>::type indices;
    lfinvoker<std::is_same<result_type, void>::value, F> fun{fun_};
    message tmp;
    auto needs_detaching = is_manipulator && xs.shared();
    if (needs_detaching)
      tmp = message::copy(xs);
    intermediate_pseudo_tuple tup{needs_detaching ? tmp.content() : xs};
    auto fun_res = apply_args(fun, indices, tup);
    return f.visit(fun_res) ? match_case::match : match_case::skip;
  }

protected:
  F fun_;
};

struct match_case_info {
  uint32_t type_token;
  match_case* ptr;
};

inline bool operator<(const match_case_info& x, const match_case_info& y) {
  return x.type_token < y.type_token;
}

template <class F>
typename std::enable_if<
  !std::is_base_of<match_case, F>::value,
  std::tuple<trivial_match_case<F>>
>::type
to_match_case_tuple(F fun) {
  return std::make_tuple(std::move(fun));
}

template <class MatchCase>
typename std::enable_if<
  std::is_base_of<match_case, MatchCase>::value,
  std::tuple<const MatchCase&>
>::type
to_match_case_tuple(const MatchCase& x) {
  return std::tie(x);
}

template <class... Ts>
const std::tuple<Ts...>& to_match_case_tuple(const std::tuple<Ts...>& x) {
  static_assert(detail::conjunction<
                  std::is_base_of<
                    match_case,
                    Ts
                  >::value...
                >::value,
                "to_match_case_tuple received a tuple of non-match_case Ts");
  return x;
}

template <class T, class U>
typename std::enable_if<
  std::is_base_of<match_case, T>::value || std::is_base_of<match_case, U>::value
>::type
operator,(T, U) {
  static_assert(!std::is_same<T, T>::value,
                "this syntax is not supported -> you probably did "
                "something like 'return (...)' instead of 'return {...}'");
}

} // namespace caf

