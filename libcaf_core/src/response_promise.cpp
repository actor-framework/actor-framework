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

#include <utility>

#include "caf/local_actor.hpp"
#include "caf/response_promise.hpp"

using std::move;

namespace caf {

response_promise::response_promise(const actor_addr& from, const actor_addr& to,
                                   const message_id& id)
    : m_from(from), m_to(to), m_id(id) {
  CAF_REQUIRE(id.is_response() || !id.valid());
}

void response_promise::deliver(message msg) const {
  if (!m_to) {
    return;
  }
  auto to = actor_cast<abstract_actor_ptr>(m_to);
  auto from = actor_cast<abstract_actor_ptr>(m_from);
  to->enqueue(m_from, m_id, move(msg), from->m_host);
}

} // namespace caf
