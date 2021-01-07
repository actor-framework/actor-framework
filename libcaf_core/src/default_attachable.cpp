// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

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

} // namespace

void default_attachable::actor_exited(const error& rsn, execution_unit* host) {
  CAF_ASSERT(observed_ != observer_);
  auto factory = type_ == monitor ? &make<down_msg> : &make<exit_msg>;
  auto observer = actor_cast<strong_actor_ptr>(observer_);
  auto observed = actor_cast<strong_actor_ptr>(observed_);
  if (observer)
    observer->enqueue(std::move(observed), make_message_id(priority_),
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
                                       observe_type type,
                                       message_priority priority)
    : observed_(std::move(observed)),
      observer_(std::move(observer)),
      type_(type),
      priority_(priority) {
  // nop
}

} // namespace caf
