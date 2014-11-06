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

#ifndef CAF_MATCH_EXPR_HPP
#define CAF_MATCH_EXPR_HPP

#include <vector>

#include "caf/none.hpp"
#include "caf/variant.hpp"

#include "caf/unit.hpp"

#include "caf/detail/int_list.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/detail/purge_refs.hpp"
#include "caf/detail/apply_args.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/detail/left_or_right.hpp"

#include "caf/detail/matcher.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/detail/lifted_fun.hpp"
#include "caf/detail/pseudo_tuple.hpp"
#include "caf/detail/behavior_impl.hpp"

namespace caf {
namespace detail {

template <long N>
struct long_constant {
  static constexpr long value = N;
};

using minus1l = long_constant<-1l>;

template <class T1, typename T2>
T2& deduce_const(T1&, T2& rhs) {
  return rhs;
}

template <class T1, typename T2>
const T2& deduce_const(const T1&, T2& rhs) {
  return rhs;
}

template <class Expr, class Projecs, class Signature, class Pattern>
class match_expr_case : public get_lifted_fun<Expr, Projecs, Signature>::type {
 public:
  using super = typename get_lifted_fun<Expr, Projecs, Signature>::type;
  template <class... Ts>
  match_expr_case(Ts&&... args) : super(std::forward<Ts>(args)...) {
    // nop
  }
  using pattern = Pattern;
  using filtered_pattern =
    typename detail::tl_filter_not_type<
    Pattern,
    anything
  >::type;
  using intermediate_tuple =
    typename detail::tl_apply<
    filtered_pattern,
    detail::pseudo_tuple
  >::type;
};

template <class Expr, class Transformers, class Pattern>
struct get_case_ {
  using ctrait = typename detail::get_callable_trait<Expr>::type;

  using filtered_pattern =
    typename detail::tl_filter_not_type<
      Pattern,
      anything
    >::type;

  using padded_transformers =
    typename detail::tl_pad_right<
      Transformers,
      detail::tl_size<filtered_pattern>::value
    >::type;

  using base_signature =
    typename detail::tl_map<
      filtered_pattern,
      std::add_const,
      std::add_lvalue_reference
    >::type;

  using padded_expr_args =
    typename detail::tl_map_conditional<
      typename detail::tl_pad_left<
        typename ctrait::arg_types,
        detail::tl_size<filtered_pattern>::value
      >::type,
      std::is_lvalue_reference,
      false,
      std::add_const,
      std::add_lvalue_reference
    >::type;

  // override base signature with required argument types of Expr
  // and result types of transformation
  using partial_fun_signature =
    typename detail::tl_zip<
      typename detail::tl_map<
        padded_transformers,
        detail::map_to_result_type,
        detail::rm_optional,
        std::add_lvalue_reference
      >::type,
      typename detail::tl_zip<
        padded_expr_args,
        base_signature,
        detail::left_or_right
      >::type,
      detail::left_or_right
    >::type;

  // 'inherit' mutable references from partial_fun_signature
  // for arguments without transformation
  using projection_signature =
    typename detail::tl_zip<
      typename detail::tl_zip<
        padded_transformers,
        partial_fun_signature,
        detail::if_not_left
      >::type,
      base_signature,
      detail::deduce_ref_type
    >::type;

  using type =
    match_expr_case<
      Expr,
      padded_transformers,
      projection_signature,
      Pattern
    >;
};

template <bool Complete, class Expr, class Trans, class Pattern>
struct get_case {
  using type = typename get_case_<Expr, Trans, Pattern>::type;
};

template <class Expr, class Trans, class Pattern>
struct get_case<false, Expr, Trans, Pattern> {
  using lhs_pattern = typename detail::tl_pop_back<Pattern>::type;
  using rhs_pattern =
    typename detail::tl_map<
      typename detail::get_callable_trait<Expr>::arg_types,
      std::decay
    >::type;
  using type =
    typename get_case_<
      Expr,
      Trans,
      typename detail::tl_concat<
        lhs_pattern,
        rhs_pattern
      >::type
    >::type;
};

template <class Fun>
struct has_bool_result {
  using result_type = typename Fun::result_type;
  static constexpr bool value = std::is_same<bool, result_type>::value;
  using token_type = std::integral_constant<bool, value>;
};

template <class T>
bool unroll_expr_result_valid(const T&) {
  return true;
}

template <class T>
bool unroll_expr_result_valid(const optional<T>& opt) {
  return static_cast<bool>(opt);
}

inline variant<none_t, unit_t> unroll_expr_result_unbox(bool& value) {
  if (value) return unit;
  return none;
}

template <class T>
T& unroll_expr_result_unbox(optional<T>& opt) {
  return *opt;
}

template <class Result, class PPFPs, class Msg>
Result unroll_expr(PPFPs&, uint64_t, minus1l, Msg&) {
  // end of recursion
  return none;
}

template <class Result, class PPFPs, long N, class Msg>
Result unroll_expr(PPFPs& fs, uint64_t bitmask, long_constant<N>, Msg& msg) {
  { // recursively evaluate sub expressions
    Result res = unroll_expr<Result>(fs, bitmask, long_constant<N - 1>{}, msg);
    if (!get<none_t>(&res)) {
      return res;
    }
  }
  if ((bitmask & (0x01 << N)) == 0) {
    // case is disabled via bitmask
    return none;
  }
  auto& f = get<N>(fs);
  using ft = typename std::decay<decltype(f)>::type;
  detail::matcher<typename ft::pattern, typename ft::filtered_pattern> match;
  typename ft::intermediate_tuple targs;
  if (match(msg, &targs)) {
  //if (policy::prepare_invoke(targs, type_token, is_dynamic, ptr, tup)) {
    auto is = detail::get_indices(targs);
    auto res = detail::apply_args(f, is, deduce_const(msg, targs));
    if (unroll_expr_result_valid(res)) {
      return std::move(unroll_expr_result_unbox(res));
    }
  }
  return none;
}

template <class PPFPs, class Tuple>
uint64_t calc_bitmask(PPFPs&, minus1l, const std::type_info&, const Tuple&) {
  return 0x00;
}

template <class Case, long N, class Tuple>
uint64_t calc_bitmask(Case& fs, long_constant<N>,
               const std::type_info& tinf, const Tuple& tup) {
  auto& f = get<N>(fs);
  using ft = typename std::decay<decltype(f)>::type;
  detail::matcher<typename ft::pattern, typename ft::filtered_pattern> match;
  uint64_t result = match(tup, nullptr) ? (0x01 << N) : 0x00;
  return result | calc_bitmask(fs, long_constant<N - 1l>(), tinf, tup);
}

template <bool IsManipulator, typename T0, typename T1>
struct mexpr_fwd_ {
  using type = T1;
};

template <class T>
struct mexpr_fwd_<false, const T&, T> {
  using type = std::reference_wrapper<const T>;
};

template <class T>
struct mexpr_fwd_<true, T&, T> {
  using type = std::reference_wrapper<T>;
};

template <bool IsManipulator, typename T>
struct mexpr_fwd {
  using type =
    typename mexpr_fwd_<
      IsManipulator,
      T,
      typename detail::implicit_conversions<
        typename std::decay<T>::type
      >::type
    >::type;
};

// detach_if_needed(message tup, bool do_detach)
inline message& detach_if_needed(message& tup, std::true_type) {
  tup.force_detach();
  return tup;
}

inline message detach_if_needed(const message& tup, std::true_type) {
  message cpy{tup};
  cpy.force_detach();
  return cpy;
}

inline const message& detach_if_needed(const message& tup, std::false_type) {
  return tup;
}

inline void* fetch_native_data(message& msg, std::true_type) {
  return msg.empty() ? nullptr : msg.vals()->mutable_native_data();
}

inline const void* fetch_native_data(const message& msg, std::false_type) {
  return msg.empty() ? nullptr : msg.vals()->native_data();
}

template <class T>
struct is_manipulator_case {
  // static constexpr bool value = T::second_type::manipulates_args;
  using arg_types = typename T::arg_types;
  static constexpr bool value = tl_exists<arg_types, is_mutable_ref>::value;

};

template <class T>
struct get_case_result {
  // using type = typename T::second_type::result_type;
  using type = typename T::result_type;

};

} // namespace detail
} // namespace caf

namespace caf {

template <class List>
struct match_result_from_type_list;

template <class... Ts>
struct match_result_from_type_list<detail::type_list<Ts...>> {
  using type = variant<none_t, typename lift_void<Ts>::type...>;

};

/**
 * A match expression encapsulating cases `Cs..., whereas
 * each case is a `detail::match_expr_case<`...>.
 */
template <class... Cs>
class match_expr {
 public:
  static_assert(sizeof...(Cs) < 64, "too many functions");

  using cases_list = detail::type_list<Cs...>;

  using result_type =
    typename match_result_from_type_list<
      typename detail::tl_distinct<
        typename detail::tl_map<
          cases_list,
          detail::get_case_result
        >::type
      >::type
    >::type;

  static constexpr bool has_manipulator =
    detail::tl_exists<cases_list, detail::is_manipulator_case>::value;

  using idx_token_type = detail::long_constant<sizeof...(Cs) - 1l>;

  template <class T, class... Ts>
  match_expr(T v, Ts&&... vs) : m_cases(std::move(v), std::forward<Ts>(vs)...) {
    init();
  }

  match_expr(match_expr&& other) : m_cases(std::move(other.m_cases)) {
    init();
  }

  match_expr(const match_expr& other) : m_cases(other.m_cases) {
    init();
  }

  result_type operator()(const message& tup) {
    return apply(tup);
  }

  result_type operator()(message& tup) {
    return apply(tup);
  }

  result_type operator()(message&& tup) {
    message tmp{tup};
    return apply(tmp);
  }

  template <class... Ds>
  match_expr<Cs..., Ds...> or_else(const match_expr<Ds...>& other) const {
    return {tuple_cat(m_cases, other.cases())};
  }

  /** @cond PRIVATE */

  const std::tuple<Cs...>& cases() const {
    return m_cases;
  }

  intrusive_ptr<detail::behavior_impl> as_behavior_impl() const {
    // return new pfun_impl(*this);
    auto lvoid = [] {};
    using impl = detail::default_behavior_impl<match_expr, decltype(lvoid)>;
    return new impl(*this, duration{}, lvoid);
  }

  /** @endcond */

 private:
  // structure: std::tuple<std::tuple<type_list<...>, ...>,
  //                       std::tuple<type_list<...>, ...>,
  //                       ...>
  std::tuple<Cs...> m_cases;

  static constexpr size_t cache_size = 10;

  using cache_element = std::pair<const std::type_info*, uint64_t>;

  std::vector<cache_element> m_cache;

  // ring buffer like access to m_cache
  size_t m_cache_begin;
  size_t m_cache_end;

  cache_element m_dummy;

  static void advance_(size_t& i) {
    i = (i + 1) % cache_size;
  }

  size_t find_token_pos(const std::type_info* type_token) {
    for (size_t i = m_cache_begin; i != m_cache_end; advance_(i)) {
      if (m_cache[i].first == type_token) {
        return i;
      }
    }
    return m_cache_end;
  }

  template <class Tuple>
  uint64_t get_cache_entry(const std::type_info* type_token,
                           const Tuple& value) {
    CAF_REQUIRE(type_token != nullptr);
    if (value.dynamically_typed()) {
      return m_dummy.second; // all groups enabled
    }
    size_t i = find_token_pos(type_token);
    // if we didn't found a cache entry ...
    if (i == m_cache_end) {
      // ... 'create' one (override oldest element in cache if full)
      advance_(m_cache_end);
      if (m_cache_end == m_cache_begin) {
        advance_(m_cache_begin);
      }
      m_cache[i].first = type_token;
      idx_token_type idx_token;
      m_cache[i].second = calc_bitmask(m_cases, idx_token, *type_token, value);
    }
    return m_cache[i].second;
  }

  void init() {
    m_dummy.second = std::numeric_limits<uint64_t>::max();
    m_cache.resize(cache_size);
    for (auto& entry : m_cache) {
      entry.first = nullptr;
    }
    m_cache_begin = m_cache_end = 0;
  }

  template <class Msg>
  result_type apply(Msg& msg) {
    idx_token_type idx_token;
    std::integral_constant<bool, has_manipulator> mutator_token;
    // returns either a reference or a new object
    using detached = decltype(detail::detach_if_needed(msg, mutator_token));
    detached mref = detail::detach_if_needed(msg, mutator_token);
    auto bitmask = get_cache_entry(mref.type_token(), mref);
    return detail::unroll_expr<result_type>(m_cases, bitmask, idx_token, mref);
  }
};

template <class T>
struct is_match_expr : std::false_type { };

template <class... Cs>
struct is_match_expr<match_expr<Cs...>> : std::true_type { };

template <class... Lhs, class... Rhs>
match_expr<Lhs..., Rhs...> operator,(const match_expr<Lhs...>& lhs,
                                     const match_expr<Rhs...>& rhs) {
  return lhs.or_else(rhs);
}

template <class... Cs>
match_expr<Cs...>& match_expr_collect(match_expr<Cs...>& arg) {
  return arg;
}

template <class... Cs>
match_expr<Cs...>&& match_expr_collect(match_expr<Cs...>&& arg) {
  return std::move(arg);
}

template <class... Cs>
const match_expr<Cs...>& match_expr_collect(const match_expr<Cs...>& arg) {
  return arg;
}

template <class T, class... Ts>
typename detail::tl_apply<
  typename detail::tl_concat<
    typename T::cases_list,
    typename Ts::cases_list...
  >::type,
  match_expr
>::type
match_expr_collect(const T& arg, const Ts&... args) {
  return {std::tuple_cat(arg.cases(), args.cases()...)};
}

namespace detail {

// implemented in message_handler.cpp
message_handler combine(behavior_impl_ptr, behavior_impl_ptr);
behavior_impl_ptr extract(const message_handler&);

template <class... Cs>
behavior_impl_ptr extract(const match_expr<Cs...>& arg) {
  return arg.as_behavior_impl();
}

template <class... As, class... Bs>
match_expr<As..., Bs...> combine(const match_expr<As...>& lhs,
                                 const match_expr<Bs...>& rhs) {
  return lhs.or_else(rhs);
}

// forwards match_expr as match_expr as long as combining two match_expr,
// otherwise turns everything into behavior_impl_ptr
template <class... As, class... Bs>
const match_expr<As...>& combine_fwd(const match_expr<As...>& lhs,
                                     const match_expr<Bs...>&) {
  return lhs;
}

template <class T, typename U>
behavior_impl_ptr combine_fwd(T& lhs, U&) {
  return extract(lhs);
}

template <class T>
behavior_impl_ptr match_expr_concat(const T& arg) {
  return arg.as_behavior_impl();
}

template <class F>
behavior_impl_ptr match_expr_concat(const message_handler& arg0,
                                    const timeout_definition<F>& arg) {
  return extract(arg0)->copy(arg);
}

template <class... Cs, typename F>
behavior_impl_ptr match_expr_concat(const match_expr<Cs...>& arg0,
                                    const timeout_definition<F>& arg) {
  return new default_behavior_impl<match_expr<Cs...>, F>{arg0, arg};
}

template <class T0, typename T1, class... Ts>
behavior_impl_ptr match_expr_concat(const T0& arg0, const T1& arg1,
                                    const Ts&... args) {
  return match_expr_concat(combine(combine_fwd(arg0, arg1),
                                   combine_fwd(arg1, arg0)),
                           args...);
}

// some more convenience functions

template <class F,
          class E = typename std::enable_if<is_callable<F>::value>::type>
match_expr<typename get_case<false, F, type_list<>, type_list<>>::type>
lift_to_match_expr(F fun) {
  using result_type =
    typename get_case<
      false,
      F,
      detail::empty_type_list,
      detail::empty_type_list
    >::type;
  return result_type{std::move(fun)};
}

template <class T,
          class E = typename std::enable_if<!is_callable<T>::value>::type>
T lift_to_match_expr(T arg) {
  return arg;
}

} // namespace detail

} // namespace caf

#endif // CAF_MATCH_EXPR_HPP
