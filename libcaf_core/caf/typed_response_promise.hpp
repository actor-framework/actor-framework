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

#ifndef CAF_TYPED_RESPONSE_PROMISE_HPP
#define CAF_TYPED_RESPONSE_PROMISE_HPP

#include "caf/either.hpp"
#include "caf/response_promise.hpp"

namespace caf {

template <class... Ts>
class typed_response_promise {
public:
  typed_response_promise() = default;
  typed_response_promise(const typed_response_promise&) = default;
  typed_response_promise& operator=(const typed_response_promise&) = default;

  typed_response_promise(response_promise promise) : promise_(promise) {
    // nop
  }

  explicit operator bool() const {
    // handle is valid if it has a receiver
    return static_cast<bool>(promise_);
  }

  void deliver(Ts... what) const {
    promise_.deliver(make_message(std::move(what)...));
  }


private:
  response_promise promise_;
};

template <class L, class R>
class typed_response_promise<either_or_t<L, R>> {
public:
  typed_response_promise() = default;
  typed_response_promise(const typed_response_promise&) = default;
  typed_response_promise& operator=(const typed_response_promise&) = default;

  typed_response_promise(response_promise promise) : promise_(promise) {
    // nop
  }

  explicit operator bool() const {
    // handle is valid if it has a receiver
    return static_cast<bool>(promise_);
  }

  void deliver(either_or_t<L, R> what) const {
    promise_.deliver(what.value);
  }


private:
  response_promise promise_;
};

} // namespace caf

#endif // CAF_TYPED_RESPONSE_PROMISE_HPP
