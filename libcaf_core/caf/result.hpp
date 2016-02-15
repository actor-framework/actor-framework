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

#ifndef CAF_RESULT_HPP
#define CAF_RESULT_HPP

#include "caf/none.hpp"
#include "caf/error.hpp"
#include "caf/message.hpp"
#include "caf/skip_message.hpp"

namespace caf {

enum result_runtime_type {
  rt_value,
  rt_error,
  rt_delegated,
  rt_skip_message
};

template <class... Ts>
struct result {
public:
  result(Ts... xs) : flag(rt_value), value(make_message(std::move(xs)...)) {
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

  result(skip_message_t) : flag(rt_skip_message) {
    // nop
  }

  result(delegated<Ts...>) : flag(rt_delegated) {
    // nop
  }

  result_runtime_type flag;
  message value;
  error err;
};

} // namespace caf

#endif // CAF_RESULT_HPP
