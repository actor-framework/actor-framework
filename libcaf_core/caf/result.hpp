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

#include "caf/fwd.hpp"
#include "caf/none.hpp"
#include "caf/skip.hpp"
#include "caf/error.hpp"
#include "caf/expected.hpp"
#include "caf/message.hpp"
#include "caf/delegated.hpp"

#include "caf/detail/type_list.hpp"

namespace caf {

enum result_runtime_type {
  rt_value,
  rt_error,
  rt_delegated,
  rt_skip
};

template <class... Ts>
class result {
public:
  result(Ts... xs) : flag(rt_value), value(make_message(std::move(xs)...)) {
    // nop
  }

  template <class U, class... Us>
  result(U x, Us... xs) : flag(rt_value) {
    init(std::move(x), std::move(xs)...);
  }

  template <class E, class = enable_if_has_make_error_t<E>>
  result(E x) : flag(rt_error), err(make_error(x)) {
    // nop
  }

  result(error x) : flag(rt_error), err(std::move(x)) {
    // nop
  }

  template <
    class T,
    class = typename std::enable_if<
      sizeof...(Ts) == 1
      && std::is_convertible<
           T,
           detail::tl_head_t<detail::type_list<Ts...>>
         >::value
    >::type
  >
  result(expected<T> x) {
    if (x) {
      flag = rt_value;
      init(std::move(*x));
    } else {
      flag = rt_error;
      err = std::move(x.error());
    }
  }

  result(skip_t) : flag(rt_skip) {
    // nop
  }

  result(delegated<Ts...>) : flag(rt_delegated) {
    // nop
  }

  result(const typed_response_promise<Ts...>&) : flag(rt_delegated) {
    // nop
  }

  result(const response_promise&) : flag(rt_delegated) {
    // nop
  }

  result_runtime_type flag;
  message value;
  error err;

private:
  void init(Ts... xs) {
    value = make_message(std::move(xs)...);
  }
};

template <>
struct result<void> {
public:
  result() : flag(rt_value) {
    // nop
  }

  result(const unit_t&) : flag(rt_value) {
    // nop
  }

  template <class E, class = enable_if_has_make_error_t<E>>
  result(E x) : flag(rt_error), err(make_error(x)) {
    // nop
  }

  result(error x) : flag(rt_error), err(std::move(x)) {
    // nop
  }

  result(expected<void> x) {
    init(x);
  }

  result(expected<unit_t> x) {
    init(x);
  }

  result(skip_t) : flag(rt_skip) {
    // nop
  }

  result(delegated<void>) : flag(rt_delegated) {
    // nop
  }

  result(delegated<unit_t>) : flag(rt_delegated) {
    // nop
  }

  result(const typed_response_promise<void>&) : flag(rt_delegated) {
    // nop
  }

  result(const typed_response_promise<unit_t>&) : flag(rt_delegated) {
    // nop
  }

  result(const response_promise&) : flag(rt_delegated) {
    // nop
  }

  result_runtime_type flag;
  message value;
  error err;

private:
  template <class T>
  void init(T& x) {
    if (x) {
      flag = rt_value;
    } else {
      flag = rt_error;
      err = std::move(x.error());
    }
  }
};

template <>
struct result<unit_t> : result<void> {

  using super = result<void>;

  using super::super;
};

template <class T>
struct is_result : std::false_type {};

template <class... Ts>
struct is_result<result<Ts...>> : std::true_type {};

} // namespace caf

