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

#include "caf/detail/behavior_impl.hpp"

#include "caf/message_handler.hpp"

namespace caf {
namespace detail {

namespace {

class combinator : public behavior_impl {
 public:
  bhvr_invoke_result invoke(message& arg) {
    auto res = first->invoke(arg);
    if (!res) return second->invoke(arg);
    return res;
  }

  bhvr_invoke_result invoke(const message& arg) {
    auto res = first->invoke(arg);
    if (!res) return second->invoke(arg);
    return res;
  }

  void handle_timeout() {
    // the second behavior overrides the timeout handling of
    // first behavior
    return second->handle_timeout();
  }

  pointer copy(const generic_timeout_definition& tdef) const {
    return new combinator(first, second->copy(tdef));
  }

  combinator(const pointer& p0, const pointer& p1)
      : behavior_impl(p1->timeout()),
        first(p0),
        second(p1) {
    // nop
  }

 private:
  pointer first;
  pointer second;
};

} // namespace <anonymous>

behavior_impl::~behavior_impl() {
  // nop
}

behavior_impl::behavior_impl(duration tout) : m_timeout(tout) {
  // nop
}

behavior_impl::pointer behavior_impl::or_else(const pointer& other) {
  CAF_REQUIRE(other != nullptr);
  return new combinator(this, other);
}

behavior_impl* new_default_behavior(duration d, std::function<void()> fun) {
  using impl = default_behavior_impl<dummy_match_expr, std::function<void()>>;
  dummy_match_expr nop;
  return new impl(nop, d, std::move(fun));
}

} // namespace detail
} // namespace caf
