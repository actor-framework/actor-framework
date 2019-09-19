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

#include "caf/detail/blocking_behavior.hpp"

namespace caf {
namespace detail {

blocking_behavior::~blocking_behavior() {
  // nop
}

blocking_behavior::blocking_behavior(behavior& x) : nested(x) {
  // nop
}

result<message> blocking_behavior::fallback(message_view&) {
  return skip;
}

duration blocking_behavior::timeout() {
  return {};
}

void blocking_behavior::handle_timeout() {
  // nop
}

} // namespace detail
} // namespace caf
