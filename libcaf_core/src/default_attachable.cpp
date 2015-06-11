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
  CAF_ASSERT(self->address() != observer_);
  auto factory = type_ == monitor ? &make<down_msg> : &make<exit_msg>;
  auto ptr = actor_cast<abstract_actor_ptr>(observer_);
  ptr->enqueue(self->address(), message_id{}.with_high_priority(),
               factory(self, reason), self->host());
}

bool default_attachable::matches(const token& what) {
  if (what.subtype != attachable::token::observer) {
    return false;
  }
  auto& ot = *reinterpret_cast<const observe_token*>(what.ptr);
  return ot.observer == observer_ && ot.type == type_;
}

default_attachable::default_attachable(actor_addr observer, observe_type type)
    : observer_(std::move(observer)),
      type_(type) {
  // nop
}

} // namespace caf
