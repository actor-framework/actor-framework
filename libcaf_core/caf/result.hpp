/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#ifndef CAF_RESULT_HPP
#define CAF_RESULT_HPP

#include "caf/fwd.hpp"
#include "caf/none.hpp"
#include "caf/skip.hpp"
#include "caf/error.hpp"
#include "caf/message.hpp"
#include "caf/delegated.hpp"

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

  template <class E,
            class = typename std::enable_if<
                      std::is_same<
                        decltype(make_error(std::declval<const E&>())),
                        error
                      >::value
                    >::type>
  result(E x) : flag(rt_error), err(make_error(x)) {
    // nop
  }

  result(error x) : flag(rt_error), err(std::move(x)) {
    // nop
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

  template <class E,
            class = typename std::enable_if<
                      std::is_same<
                        decltype(make_error(std::declval<const E&>())),
                        error
                      >::value
                    >::type>
  result(E x) : flag(rt_error), err(make_error(x)) {
    // nop
  }

  result(error x) : flag(rt_error), err(std::move(x)) {
    // nop
  }

  result(skip_t) : flag(rt_skip) {
    // nop
  }

  result(delegated<void>) : flag(rt_delegated) {
    // nop
  }

  result(const typed_response_promise<void>&) : flag(rt_delegated) {
    // nop
  }

  result(const response_promise&) : flag(rt_delegated) {
    // nop
  }

  result_runtime_type flag;
  message value;
  error err;
};

} // namespace caf

#endif // CAF_RESULT_HPP
