/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
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

#include "caf/actor.hpp"
#include "caf/message.hpp"
#include "caf/actor_cast.hpp"
#include "caf/system_messages.hpp"

namespace caf {

namespace {

template <class MsgType>
message make(abstract_actor* self, const error& reason) {
  return make_message(MsgType{self->address(), reason});
}

} // namespace <anonymous>

void default_attachable::actor_exited(const error& rsn, execution_unit* host) {
  CAF_ASSERT(observed_ != observer_);
  auto factory = type_ == monitor ? &make<down_msg> : &make<exit_msg>;
  auto observer = actor_cast<strong_actor_ptr>(observer_);
  auto observed = actor_cast<strong_actor_ptr>(observed_);
  if (observer)
    observer->enqueue(std::move(observed), make_message_id(),
                      factory(actor_cast<abstract_actor*>(observed_), rsn),
                      host);
}

bool default_attachable::matches(const token& what) {
  if (what.subtype != attachable::token::observer)
    return false;
  auto& ot = *reinterpret_cast<const observe_token*>(what.ptr);
  return ot.observer == observer_ && ot.type == type_;
}

default_attachable::default_attachable(actor_addr observed, actor_addr observer,
                                       observe_type type)
    : observed_(std::move(observed)),
      observer_(std::move(observer)),
      type_(type) {
  // nop
}

} // namespace caf
