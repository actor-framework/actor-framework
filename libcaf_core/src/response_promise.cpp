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

#include <utility>

#include "caf/local_actor.hpp"
#include "caf/response_promise.hpp"

namespace caf {

response_promise::response_promise(local_actor* self, const actor_addr& to,
                                   const message_id& id)
    : self_(self), to_(to), id_(id) {
  CAF_ASSERT(id.is_response() || ! id.valid());
}

void response_promise::deliver_impl(message msg) const {
  if (! to_)
    return;
  to_->enqueue(self_->address(), id_, std::move(msg), self_->context());
}

void response_promise::deliver(error x) const {
  if (id_.valid())
    deliver_impl(make_message(std::move(x)));
}

} // namespace caf
