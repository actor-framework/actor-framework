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

#ifndef CAF_DETAIL_LIFTED_FUN_HPP
#define CAF_DETAIL_LIFTED_FUN_HPP

#include "caf/none.hpp"
#include "caf/optional.hpp"

#include "caf/skip_message.hpp"

#include "caf/detail/int_list.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/detail/tuple_zip.hpp"
#include "caf/detail/apply_args.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/detail/left_or_right.hpp"

namespace caf {
namespace detail {

class lifted_fun_zipper {

 public:

  template <class F, typename T>
  auto operator()(const F& fun, T& arg) -> decltype(fun(arg)) const {
    return fun(arg);
  }

  // forward everything as reference if no guard/transformation is set
  template <class T>
  auto operator()(const unit_t&, T& arg) const -> decltype(std::ref(arg)) {
    return std::ref(arg);
  }

};

template <class T>
T& unopt(T& v) {
  return v;
}

template <class T>
T& unopt(optional<T>& v) {
  return *v;
}

inline bool has_none() {
  return false;
}

template <class T, class... Ts>
inline bool has_none(const T&, const Ts&... vs) {
  return has_none(vs...);
}

template <class T, class... Ts>
inline bool has_none(const optional<T>& v, const Ts&... vs) {
  return !v || has_none(vs...);
}

// allows F to have fewer arguments than the lifted_fun calling it
template <class R, typename F>
class lifted_fun_invoker {

  using arg_types = typename get_callable_trait<F>::arg_types;

  static constexpr size_t args = tl_size<arg_types>::value;

 public:

  lifted_fun_invoker(F& fun) : f(fun) {}

  template <class... Ts>
  typename std::enable_if<sizeof...(Ts) == args, R>::type
  operator()(Ts&... args) const {
    if (has_none(args...)) return none;
    return f(unopt(args)...);
  }

  template <class T, class... Ts>
  typename std::enable_if<(sizeof...(Ts) + 1 > args), R>::type
  operator()(T& arg, Ts&... args) const {
    if (has_none(arg)) return none;
    return (*this)(args...);
  }

 private:

  F& f;

};

template <class F>
class lifted_fun_invoker<bool, F> {

  using arg_types = typename get_callable_trait<F>::arg_types;

  static constexpr size_t args = tl_size<arg_types>::value;

 public:

  lifted_fun_invoker(F& fun) : f(fun) {}

  template <class... Ts>
  typename std::enable_if<sizeof...(Ts) == args, bool>::type
  operator()(Ts&&... args) const {
    if (has_none(args...)) return false;
    f(unopt(args)...);
    return true;
  }

  template <class T, class... Ts>
  typename std::enable_if<(sizeof...(Ts) + 1 > args), bool>::type
  operator()(T&& arg, Ts&&... args) const {
    if (has_none(arg)) return false;
    return (*this)(args...);
  }

 private:

  F& f;

};

/**
 * A lifted functor consists of a set of projections, a plain-old
 * functor and its signature. Note that the signature of the lifted
 * functor might differ from the underlying functor, because
 * of the projections.
 */
template <class F, class ListOfProjections, class... Args>
class lifted_fun {

 public:

  using result_type = typename get_callable_trait<F>::result_type;

  // Let F be "R (Ts...)" then lifted_fun<F...> returns optional<R>
  // unless R is void in which case bool is returned
  using optional_result_type =
    typename std::conditional<
      std::is_same<result_type, void>::value,
      bool,
      optional<result_type>
    >::type;

  using projections_list = ListOfProjections;

  using projections = typename tl_apply<projections_list, std::tuple>::type;

  using arg_types = type_list<Args...>;

  lifted_fun() = default;

  lifted_fun(const lifted_fun&) = default;

  lifted_fun& operator=(lifted_fun&&) = default;

  lifted_fun& operator=(const lifted_fun&) = default;

  lifted_fun(F f) : m_fun(std::move(f)) {}

  lifted_fun(F f, projections ps) : m_fun(std::move(f)), m_ps(std::move(ps)) {
    // nop
  }

  /**
   * Invokes `fun` with a lifted_fun of `args....
   */
  optional_result_type operator()(Args... args) {
    auto indices = get_indices(m_ps);
    lifted_fun_zipper zip;
    lifted_fun_invoker<optional_result_type, F> invoke{m_fun};
    return apply_args(invoke, indices,
                      tuple_zip(zip, indices, m_ps,
                                std::forward_as_tuple(args...)));
  }

 private:
  F m_fun;
  projections m_ps;
};

template <class F, class ListOfProjections, class List>
struct get_lifted_fun;

template <class F, class ListOfProjections, class... Ts>
struct get_lifted_fun<F, ListOfProjections, type_list<Ts...>> {
  using type = lifted_fun<F, ListOfProjections, Ts...>;
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_LIFTED_FUN_HPP
