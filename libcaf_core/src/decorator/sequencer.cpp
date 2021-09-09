// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/decorator/sequencer.hpp"

#include "caf/actor_system.hpp"
#include "caf/default_attachable.hpp"
#include "caf/detail/sync_request_bouncer.hpp"

namespace caf::decorator {

sequencer::sequencer(strong_actor_ptr f, strong_actor_ptr g,
                     message_types_set msg_types)
  : monitorable_actor(actor_config{}.add_flag(is_actor_dot_decorator_flag)),
    f_(std::move(f)),
    g_(std::move(g)),
    msg_types_(std::move(msg_types)) {
  CAF_ASSERT(f_);
  CAF_ASSERT(g_);
  // composed actor has dependency on constituent actors by default;
  // if either constituent actor is already dead upon establishing
  // the dependency, the actor is spawned dead
  auto monitor1
    = default_attachable::make_monitor(actor_cast<actor_addr>(f_), address());
  f_->get()->attach(std::move(monitor1));
  if (g_ != f_) {
    auto monitor2
      = default_attachable::make_monitor(actor_cast<actor_addr>(g_), address());
    g_->get()->attach(std::move(monitor2));
  }
}

bool sequencer::enqueue(mailbox_element_ptr what, execution_unit* context) {
  auto down_msg_handler = [&](down_msg& dm) {
    // quit if either `f` or `g` are no longer available
    cleanup(std::move(dm.reason), context);
  };
  if (handle_system_message(*what, context, false, down_msg_handler))
    return true;
  strong_actor_ptr f;
  strong_actor_ptr g;
  error err;
  shared_critical_section([&] {
    f = f_;
    g = g_;
    err = fail_state_;
  });
  if (!f) {
    // f and g are invalid only after the sequencer terminated
    bounce(what, err);
    return false;
  }
  // process and forward the non-system message;
  // store `f` as the next stage in the forwarding chain
  what->stages.push_back(std::move(f));
  // forward modified message to `g`
  return g->enqueue(std::move(what), context);
}

sequencer::message_types_set sequencer::message_types() const {
  return msg_types_;
}

void sequencer::on_cleanup(const error&) {
  f_.reset();
  g_.reset();
}

} // namespace caf::decorator
