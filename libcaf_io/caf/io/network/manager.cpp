// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/io/network/manager.hpp"

#include "caf/io/abstract_broker.hpp"

#include "caf/log/io.hpp"

namespace caf::io::network {

manager::manager() : parent_(nullptr) {
  // nop
}

manager::~manager() {
  // nop
}

void manager::set_parent(abstract_broker* ptr) {
  parent_ = ptr != nullptr ? ptr->ctrl() : nullptr;
}

abstract_broker* manager::parent() {
  return parent_ ? static_cast<abstract_broker*>(parent_->get()) : nullptr;
}

void manager::detach(execution_unit*, bool invoke_disconnect_message) {
  CAF_LOG_TRACE(CAF_ARG(invoke_disconnect_message));
  // This function gets called from the multiplexer when an error occurs or
  // from the broker when closing this manager. In both cases, we need to make
  // sure this manager does not receive further socket events.
  remove_from_loop();
  // Disconnect from the broker if not already detached.
  if (!detached()) {
    log::io::debug("disconnect servant from broker");
    auto raw_ptr = parent();
    // Keep a strong reference to our parent until we go out of scope.
    strong_actor_ptr ptr;
    ptr.swap(parent_);
    detach_from(raw_ptr);
    if (invoke_disconnect_message) {
      auto mptr = make_mailbox_element(nullptr, make_message_id(),
                                       detach_message());
      switch (raw_ptr->consume(*mptr)) {
        case invoke_message_result::consumed:
          raw_ptr->finalize();
          break;
        case invoke_message_result::skipped:
          raw_ptr->push_to_cache(std::move(mptr));
          break;
        case invoke_message_result::dropped:
          log::io::info("broker dropped disconnect message");
          break;
      }
    }
  }
}

void manager::io_failure(execution_unit* ctx, operation op) {
  CAF_LOG_TRACE(CAF_ARG(op));
  CAF_IGNORE_UNUSED(op);
  detach(ctx, true);
}

} // namespace caf::io::network
