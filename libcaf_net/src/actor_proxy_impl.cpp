// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/actor_proxy_impl.hpp"

#include "caf/actor_system.hpp"
#include "caf/expected.hpp"
#include "caf/logger.hpp"

namespace caf::net {

actor_proxy_impl::actor_proxy_impl(actor_config& cfg, endpoint_manager_ptr dst)
  : super(cfg), dst_(std::move(dst)) {
  CAF_ASSERT(dst_ != nullptr);
  dst_->enqueue_event(node(), id());
}

actor_proxy_impl::~actor_proxy_impl() {
  // nop
}

void actor_proxy_impl::enqueue(mailbox_element_ptr msg, execution_unit*) {
  CAF_PUSH_AID(0);
  CAF_ASSERT(msg != nullptr);
  CAF_LOG_SEND_EVENT(msg);
  dst_->enqueue(std::move(msg), ctrl());
}

void actor_proxy_impl::kill_proxy(execution_unit* ctx, error rsn) {
  cleanup(std::move(rsn), ctx);
}

} // namespace caf::net
