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

#include "caf/actor.hpp"
#include "caf/channel.hpp"
#include "caf/actor_addr.hpp"
#include "caf/actor_proxy.hpp"
#include "caf/local_actor.hpp"
#include "caf/blocking_actor.hpp"
#include "caf/event_based_actor.hpp"

namespace caf {

actor::actor(const invalid_actor_t&) : m_ptr(nullptr) {
  // nop
}

actor::actor(abstract_actor* ptr) : m_ptr(ptr) {
  // nop
}

actor& actor::operator=(const invalid_actor_t&) {
  m_ptr.reset();
  return *this;
}

intptr_t actor::compare(const actor& other) const {
  return channel::compare(m_ptr.get(), other.m_ptr.get());
}

intptr_t actor::compare(const actor_addr& other) const {
  return static_cast<ptrdiff_t>(m_ptr.get() - other.m_ptr.get());
}

void actor::swap(actor& other) {
  m_ptr.swap(other.m_ptr);
}

actor_addr actor::address() const {
  return m_ptr ? m_ptr->address() : actor_addr{};
}

actor_id actor::id() const {
  return (m_ptr) ? m_ptr->id() : 0;
}

} // namespace caf
