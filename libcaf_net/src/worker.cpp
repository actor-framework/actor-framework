/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/net/basp/worker.hpp"

#include "caf/actor_system.hpp"
#include "caf/byte.hpp"
#include "caf/net/basp/message_queue.hpp"
#include "caf/proxy_registry.hpp"
#include "caf/scheduler/abstract_coordinator.hpp"

namespace caf {
namespace net {
namespace basp {

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
                    span<const byte> payload) {
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

} // namespace basp
} // namespace net
} // namespace caf
