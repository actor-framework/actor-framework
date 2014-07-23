/******************************************************************************
 *                   _________________________                                *
 *                   __  ____/__    |__  ____/    C++                         *
 *                   _  /    __  /| |_  /_        Actor                       *
 *                   / /___  _  ___ |  __/        Framework                   *
 *                   ____/  /_/  |_/_/                                       *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENCE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "cppa/to_string.hpp"

#include "cppa/config.hpp"
#include "cppa/message_handler.hpp"

namespace cppa {

message_handler::message_handler(impl_ptr ptr) : m_impl(std::move(ptr)) {}

void detail::behavior_impl::handle_timeout() {}

} // namespace cppa

namespace cppa {
namespace detail {

message_handler combine(behavior_impl_ptr lhs, behavior_impl_ptr rhs) {
  return {lhs->or_else(rhs)};
}

behavior_impl_ptr extract(const message_handler& arg) {
  return arg.as_behavior_impl();
}
}
} // namespace cppa::detail
