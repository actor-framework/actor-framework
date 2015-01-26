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

#include "caf/actor.hpp"
#include "caf/group.hpp"
#include "caf/channel.hpp"
#include "caf/message.hpp"
#include "caf/actor_cast.hpp"

namespace caf {

channel::channel(const actor& other)
    : m_ptr(actor_cast<abstract_channel_ptr>(other)) {
  // nop
}

channel::channel(const group& other) : m_ptr(other.ptr()) {
  // nop
}

channel::channel(const invalid_actor_t&) : m_ptr(nullptr) {
  // nop
}

channel::channel(const invalid_group_t&) : m_ptr(nullptr) {
  // nop
}

intptr_t channel::compare(const abstract_channel* lhs,
                          const abstract_channel* rhs) {
  return reinterpret_cast<intptr_t>(lhs) - reinterpret_cast<intptr_t>(rhs);
}

channel::channel(abstract_channel* ptr) : m_ptr(ptr) {}

intptr_t channel::compare(const channel& other) const {
  return compare(m_ptr.get(), other.m_ptr.get());
}

intptr_t channel::compare(const actor& other) const {
  return compare(m_ptr.get(), actor_cast<abstract_actor_ptr>(other).get());
}

intptr_t channel::compare(const abstract_channel* other) const {
  return compare(m_ptr.get(), other);
}

} // namespace caf
