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
