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
 * License 1.0. See accompanying files LICENSE and LICENCE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_DETAIL_RESPONSE_FUTURE_DETAIL_HPP
#define CAF_DETAIL_RESPONSE_FUTURE_DETAIL_HPP

#include "caf/on.hpp"
#include "caf/skip_message.hpp"
#include "caf/system_messages.hpp"

#include "caf/detail/type_traits.hpp"

namespace caf {
namespace detail {

template <class Actor, class... Fs>
behavior fs2bhvr(Actor* self, Fs... fs) {
  auto handle_sync_timeout = [self]() -> skip_message_t {
    self->handle_sync_timeout();
    return {};
  };
  return behavior{
    on<sync_timeout_msg>() >> handle_sync_timeout,
    on<unit_t>() >> skip_message,
    on<sync_exited_msg>() >> skip_message,
    (on(any_vals, arg_match) >> std::move(fs))...
  };
}

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_RESPONSE_FUTURE_DETAIL_HPP
