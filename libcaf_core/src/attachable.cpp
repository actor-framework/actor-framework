// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/attachable.hpp"

#include "caf/actor_cast.hpp"
#include "caf/default_attachable.hpp"
#include "caf/system_messages.hpp"

namespace caf {

attachable::~attachable() {
  // Avoid recursive cleanup of next pointers because this can cause a stack
  // overflow for long linked lists.
  using std::swap;
  while (next != nullptr) {
    attachable_ptr tmp;
    swap(next->next, tmp);
    swap(next, tmp);
  }
}

attachable::token::token(size_t typenr, const void* vptr)
  : subtype(typenr), ptr(vptr) {
  // nop
}

void attachable::actor_exited(const error&, execution_unit*) {
  // nop
}

bool attachable::matches(const token&) {
  return false;
}

attachable_ptr attachable::make_monitor(actor_addr observed,
                                        actor_addr observer,
                                        message_priority prio) {
  return default_attachable::make_monitor(std::move(observed),
                                          std::move(observer), prio);
}

attachable_ptr attachable::make_link(actor_addr observed, actor_addr observer) {
  return default_attachable::make_link(std::move(observed),
                                       std::move(observer));
}

namespace {

class stream_aborter : public attachable {
public:
  stream_aborter(actor_addr observed, actor_addr observer,
                 uint64_t sink_flow_id)
    : observed_(std::move(observed)),
      observer_(std::move(observer)),
      sink_flow_id_(sink_flow_id) {
    // nop
  }

  void actor_exited(const error& rsn, execution_unit* host) override {
    if (auto observer = actor_cast<strong_actor_ptr>(observer_)) {
      auto observed = actor_cast<strong_actor_ptr>(observed_);
      observer->enqueue(std::move(observed), make_message_id(),
                        make_message(stream_abort_msg{sink_flow_id_, rsn}),
                        host);
    }
  }

private:
  /// Holds a weak reference to the observed actor.
  actor_addr observed_;

  /// Holds a weak reference to the observing actor.
  actor_addr observer_;

  /// Identifies the aborted flow at the observer.
  uint64_t sink_flow_id_;
};

} // namespace

attachable_ptr attachable::make_stream_aborter(actor_addr observed,
                                               actor_addr observer,
                                               uint64_t sink_flow_id) {
  return std::make_unique<stream_aborter>(std::move(observed),
                                          std::move(observer), sink_flow_id);
}

} // namespace caf
