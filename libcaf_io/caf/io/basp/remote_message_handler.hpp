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
#include "caf/io/basp/header.hpp"
#include "caf/logger.hpp"
#include "caf/message.hpp"
#include "caf/message_id.hpp"
#include "caf/node_id.hpp"

namespace caf::io::basp {

template <class Subtype>
class remote_message_handler {
public:
  void handle_remote_message(execution_unit* ctx) {
    CAF_LOG_TRACE("");
    // Local variables.
    auto& dref = static_cast<Subtype&>(*this);
    auto& sys = *dref.system_;
    strong_actor_ptr src;
    strong_actor_ptr dst;
    std::vector<strong_actor_ptr> stages;
    message msg;
    auto mid = make_message_id(dref.hdr_.operation_data);
    binary_deserializer source{ctx, dref.payload_};
    // Make sure to drop the message in case we return abnormally.
    auto guard = detail::make_scope_guard(
      [&] { dref.queue_->drop(ctx, dref.msg_id_); });
    // Registry setup.
    dref.proxies_->set_last_hop(&dref.last_hop_);
    // Get the local receiver.
    if (dref.hdr_.has(basp::header::named_receiver_flag))
      dst = sys.registry().get(static_cast<atom_value>(dref.hdr_.dest_actor));
    else
      dst = sys.registry().get(dref.hdr_.dest_actor);
    // Short circuit if we already know there's nothing to do.
    if (dst == nullptr && !mid.is_request()) {
      CAF_LOG_INFO("drop asynchronous remote message: unknown destination");
      return;
    }
    // Deserialize source and destination node for routed messages.
    if (dref.hdr_.operation == basp::message_type::routed_message) {
      node_id src_node;
      node_id dst_node;
      if (auto err = source(src_node, dst_node)) {
        CAF_LOG_ERROR("cannot read source and destination of remote message");
        return;
      }
      CAF_ASSERT(dst_node == sys.node());
      if (dref.hdr_.source_actor != 0) {
        src = src_node == sys.node()
                ? sys.registry().get(dref.hdr_.source_actor)
                : dref.proxies_->get_or_put(src_node, dref.hdr_.source_actor);
      }
    } else {
      CAF_ASSERT(dref.hdr_.operation == basp::message_type::direct_message);
      src = dref.proxies_->get_or_put(dref.last_hop_, dref.hdr_.source_actor);
    }
    // Send errors for dropped requests.
    if (dst == nullptr) {
      CAF_ASSERT(mid.is_request());
      CAF_LOG_INFO("drop remote request: unknown destination");
      detail::sync_request_bouncer srb{exit_reason::remote_link_unreachable};
      srb(src, mid);
      return;
    }
    // Get the remainder of the message.
    if (auto err = source(stages, msg)) {
      CAF_LOG_ERROR("cannot read stages and content of remote message");
      return;
    }
    // Intercept link messages. Forwarding actor proxies signalize linking
    // by sending link_atom/unlink_atom message with src == dest.
    if (msg.type_token() == make_type_token<atom_value, strong_actor_ptr>()) {
      const auto& ptr = msg.get_as<strong_actor_ptr>(1);
      switch (static_cast<uint64_t>(msg.get_as<atom_value>(0))) {
        default:
          break;
        case link_atom::uint_value(): {
          if (ptr != nullptr)
            static_cast<actor_proxy*>(ptr->get())->add_link(dst->get());
          else
            CAF_LOG_WARNING("received link message with invalid target");
          return;
        }
        case unlink_atom::uint_value(): {
          if (ptr != nullptr)
            static_cast<actor_proxy*>(ptr->get())->remove_link(dst->get());
          else
            CAF_LOG_DEBUG("received unlink message with invalid target");
          return;
        }
      }
    }
    // Ship the message.
    guard.disable();
    dref.queue_->push(ctx, dref.msg_id_, std::move(dst),
                      make_mailbox_element(std::move(src), mid,
                                           std::move(stages), std::move(msg)));
  }
};

} // namespace caf
