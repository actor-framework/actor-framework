// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/io/basp/worker.hpp"

#include "caf/io/basp/message_queue.hpp"

#include "caf/actor_system.hpp"
#include "caf/proxy_registry.hpp"
#include "caf/scheduler.hpp"

namespace caf::io::basp {

// -- constructors, destructors, and assignment operators ----------------------

worker::worker(hub_type& hub, message_queue& queue, proxy_registry& proxies)
  : hub_(&hub), queue_(&queue), proxies_(&proxies), system_(&proxies.system()) {
  CAF_IGNORE_UNUSED(pad_);
}

worker::~worker() {
  // nop
}

// -- management ---------------------------------------------------------------

void worker::launch(const node_id& last_hop, const basp::header& hdr,
                    const byte_buffer& payload) {
  CAF_ASSERT(hdr.dest_actor != 0);
  CAF_ASSERT(hdr.operation == basp::message_type::direct_message
             || hdr.operation == basp::message_type::routed_message);
  msg_id_ = queue_->new_id();
  last_hop_ = last_hop;
  memcpy(&hdr_, &hdr, sizeof(basp::header));
  payload_.assign(payload.begin(), payload.end());
  ref();
  system_->scheduler().enqueue(this);
}

// -- implementation of resumable ----------------------------------------------

resumable::resume_result worker::resume(execution_unit* ctx, size_t) {
  ctx->proxy_registry_ptr(proxies_);
  handle_remote_message(ctx);
  hub_->push(this);
  return resumable::awaiting_message;
}

} // namespace caf::io::basp
