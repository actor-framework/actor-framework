// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/response_promise_state.hpp"

#include "caf/detail/actor_system_access.hpp"
#include "caf/local_actor.hpp"
#include "caf/log/core.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/message.hpp"
#include "caf/message_id.hpp"
#include "caf/sec.hpp"

namespace caf::detail {

response_promise_state::~response_promise_state() {
  // Note: the state may get destroyed outside of the actor. For example, when
  //       storing the promise in a run-later continuation. Hence, we can't call
  //       deliver_impl here since it calls self->context().
  if (self && source && !id.is_answered()) {
    log::core::debug("broken promise!");
    auto element = make_mailbox_element(self, id.response_id(),
                                        make_error(sec::broken_promise));
    source->enqueue(std::move(element), nullptr);
  }
}

void response_promise_state::cancel() {
  id.mark_as_answered();
}

void response_promise_state::deliver_impl(message msg) {
  auto lg = log::core::trace("msg = {}", msg);
  // Even though we are holding a weak pointer, we can access the pointer
  // without any additional check here because only the actor itself is allowed
  // to call this function.
  auto selfptr = static_cast<local_actor*>(self->get());
  if (msg.empty() && id.is_async()) {
    log::core::debug("drop response: empty response to asynchronous input");
  } else if (source != nullptr) {
    auto element = make_mailbox_element(self, id.response_id(), std::move(msg));
    source->enqueue(std::move(element), selfptr->context());
  } else {
    detail::actor_system_access access{selfptr->home_system()};
    access.message_rejected(nullptr);
  }
  cancel();
}

void response_promise_state::delegate_impl(abstract_actor* receiver,
                                           message msg) {
  auto lg = log::core::trace("msg = {}", msg);
  if (receiver != nullptr) {
    auto selfptr = static_cast<local_actor*>(self->get());
    auto element = make_mailbox_element(source, id, std::move(msg));
    receiver->enqueue(std::move(element), selfptr->context());
  } else {
    log::core::debug("drop response: invalid delegation target");
  }
  cancel();
}

} // namespace caf::detail
