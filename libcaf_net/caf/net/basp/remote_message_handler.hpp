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

#pragma once

#include <vector>

#include "caf/actor_control_block.hpp"
#include "caf/actor_proxy.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/config.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/detail/sync_request_bouncer.hpp"
#include "caf/execution_unit.hpp"
#include "caf/logger.hpp"
#include "caf/message.hpp"
#include "caf/message_id.hpp"
#include "caf/net/basp/header.hpp"
#include "caf/node_id.hpp"

namespace caf::net::basp {

template <class Subtype>
class remote_message_handler {
public:
  void handle_remote_message(execution_unit* ctx) {
    // Local variables.
    auto& dref = static_cast<Subtype&>(*this);
    auto& payload = dref.payload_;
    auto& hdr = dref.hdr_;
    auto& registry = dref.system_->registry();
    auto& proxies = *dref.proxies_;
    CAF_LOG_TRACE(CAF_ARG(hdr) << CAF_ARG2("payload.size", payload.size()));
    // Deserialize payload.
    actor_id src_id = 0;
    node_id src_node;
    actor_id dst_id = 0;
    std::vector<strong_actor_ptr> fwd_stack;
    message content;
    binary_deserializer source{ctx, payload};
    if (!source.apply_objects(src_node, src_id, dst_id, fwd_stack, content)) {
      CAF_LOG_ERROR(
        "failed to deserialize payload:" << CAF_ARG(source.get_error()));
      return;
    }
    // Sanity checks.
    if (dst_id == 0)
      return;
    // Try to fetch the receiver.
    auto dst_hdl = registry.get(dst_id);
    if (dst_hdl == nullptr) {
      CAF_LOG_DEBUG("no actor found for given ID, drop message");
      return;
    }
    // Try to fetch the sender.
    strong_actor_ptr src_hdl;
    if (src_node != none && src_id != 0)
      src_hdl = proxies.get_or_put(src_node, src_id);
    // Ship the message.
    auto ptr = make_mailbox_element(std::move(src_hdl),
                                    make_message_id(hdr.operation_data),
                                    std::move(fwd_stack), std::move(content));
    dref.queue_->push(ctx, dref.msg_id_, std::move(dst_hdl), std::move(ptr));
  }
};

} // namespace caf::net::basp
