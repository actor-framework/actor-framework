// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/default_attachable.hpp"

#include "caf/actor.hpp"
#include "caf/actor_cast.hpp"
#include "caf/detail/assert.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/message.hpp"
#include "caf/system_messages.hpp"

namespace caf {

namespace {

template <class MsgType>
message make(abstract_actor* self, const error& reason) {
  return make_message(MsgType{self->address(), reason});
}

} // namespace

void default_attachable::actor_exited(const error& rsn, scheduler* sched) {
  CAF_ASSERT(observed_ != observer_);
  auto factory = type_ == monitor ? &make<down_msg> : &make<exit_msg>;
  auto observer = actor_cast<strong_actor_ptr>(observer_);
  if (!observer) {
    return;
  }
  auto observed = actor_cast<strong_actor_ptr>(observed_);
  auto msg = factory(actor_cast<abstract_actor*>(observed_), rsn);
  auto ptr = make_mailbox_element(std::move(observed),
                                  make_message_id(priority_), std::move(msg));
  observer->enqueue(std::move(ptr), sched);
}

bool default_attachable::matches(const token& what) {
  if (what.subtype != attachable::token::observer)
    return false;
  auto& ref = *reinterpret_cast<const observe_token*>(what.ptr);
  return ref.observer == observer_ && ref.type == type_;
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
