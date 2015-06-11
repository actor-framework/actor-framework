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

response_promise::response_promise(const actor_addr& from, const actor_addr& to,
                                   const message_id& id)
    : from_(from), to_(to), id_(id) {
  CAF_ASSERT(id.is_response() || ! id.valid());
}

void response_promise::deliver(message msg) const {
  if (! to_) {
    return;
  }
  auto to = actor_cast<abstract_actor_ptr>(to_);
  auto from = actor_cast<abstract_actor_ptr>(from_);
  to->enqueue(from_, id_, std::move(msg), from->host());
}

} // namespace caf
