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

#include "caf/default_attachable.hpp"

#include "caf/message.hpp"
#include "caf/system_messages.hpp"
#include "caf/actor_cast.hpp"

namespace caf {

namespace {

template <class MsgType>
message make(abstract_actor* self, uint32_t reason) {
  return make_message(MsgType{self->address(), reason});
}

} // namespace <anonymous>

void default_attachable::actor_exited(abstract_actor* self, uint32_t reason) {
  CAF_REQUIRE(self->address() != m_observer);
  auto factory = m_type == monitor ? &make<down_msg> : &make<exit_msg>;
  message msg = factory(self, reason);
  auto ptr = actor_cast<abstract_actor_ptr>(m_observer);
  ptr->enqueue(self->address(), message_id{}.with_high_priority(),
               msg, self->host());
}

bool default_attachable::matches(const token& what) {
  if (what.subtype != typeid(observe_token)) {
    return false;
  }
  auto& ot = *reinterpret_cast<const observe_token*>(what.ptr);
  return ot.observer == m_observer && ot.type == m_type;
}

default_attachable::default_attachable(actor_addr observer, observe_type type)
    : m_observer(std::move(observer)),
      m_type(type) {
  // nop
}

} // namespace caf
