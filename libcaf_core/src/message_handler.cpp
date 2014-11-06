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

#include "caf/to_string.hpp"

#include "caf/config.hpp"
#include "caf/message_handler.hpp"

namespace caf {

message_handler::message_handler(impl_ptr ptr) : m_impl(std::move(ptr)) {
  // nop
}

void detail::behavior_impl::handle_timeout() {
  // nop
}

} // namespace caf

namespace caf {
namespace detail {

behavior_impl_ptr combine(behavior_impl_ptr lhs, const message_handler& rhs) {
  return lhs->or_else(rhs.as_behavior_impl());
}

behavior_impl_ptr combine(const message_handler& lhs, behavior_impl_ptr rhs) {
  return lhs.as_behavior_impl()->or_else(rhs);
}

message_handler combine(behavior_impl_ptr lhs, behavior_impl_ptr rhs) {
  return lhs->or_else(rhs);
}

behavior_impl_ptr extract(const message_handler& arg) {
  return arg.as_behavior_impl();
}

} // namespace util
} // namespace caf
