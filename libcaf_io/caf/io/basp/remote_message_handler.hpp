// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/io/basp/header.hpp"
#include "caf/io/middleman.hpp"

#include "caf/actor_control_block.hpp"
#include "caf/actor_proxy.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/config.hpp"
#include "caf/const_typed_message_view.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/detail/sync_request_bouncer.hpp"
#include "caf/log/io.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/message.hpp"
#include "caf/message_id.hpp"
#include "caf/node_id.hpp"
#include "caf/scheduler.hpp"
#include "caf/telemetry/histogram.hpp"
#include "caf/telemetry/timer.hpp"

#include <cstdint>
#include <vector>

namespace caf::io::basp {

template <class Subtype>
class remote_message_handler {
public:
  void handle_remote_message(actor_system& sys, scheduler* ctx) {
    auto lg = log::io::trace("");
    // Local variables.
    auto& dref = static_cast<Subtype&>(*this);
    strong_actor_ptr src;
    strong_actor_ptr dst;
    message msg;
    auto mid = make_message_id(dref.hdr_.operation_data);
    binary_deserializer source{sys, dref.payload_};
    // Make sure to drop the message in case we return abnormally.
    auto guard = detail::scope_guard{
      [&]() noexcept { dref.queue_->drop(ctx, dref.msg_id_); }};
    // Registry setup.
    dref.proxies_->set_last_hop(&dref.last_hop_);
    // Get the local receiver.
    if (dref.hdr_.has(basp::header::named_receiver_flag)) {
      // TODO: consider replacing hacky workaround (requires changing BASP).
      if (dref.hdr_.dest_actor == 1) {
        dst = sys.registry().get("ConfigServ");
      } else if (dref.hdr_.dest_actor == 2) {
        dst = sys.registry().get("SpawnServ");
      }
    } else {
      dst = sys.registry().get(dref.hdr_.dest_actor);
    }
    // Short circuit if we already know there's nothing to do.
    if (dst == nullptr && !mid.is_request()) {
      log::io::info("drop asynchronous remote message: unknown destination");
      return;
    }
    // Deserialize source and destination node for routed messages.
    if (dref.hdr_.operation == basp::message_type::routed_message) {
      node_id src_node;
      node_id dst_node;
      if (!source.apply(src_node)) {
        log::io::error("failed to read source of routed message: {}",
                       source.get_error());
        return;
      }
      if (!source.apply(dst_node)) {
        log::io::error("failed to read destination of routed message: {}",
                       source.get_error());
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
      log::io::info("drop remote request: unknown destination");
      detail::sync_request_bouncer srb{exit_reason::remote_link_unreachable};
      srb(src, mid);
      return;
    }
    // Get the remainder of the message.
    auto& mm_metrics = sys.middleman().metric_singletons;
    auto t0 = telemetry::timer::clock_type::now();
    if (!source.apply(msg)) {
      log::io::error("failed to read message content: {}", source.get_error());
      auto ptr = make_mailbox_element(nullptr, mid.response_id(),
                                      make_error(sec::malformed_message));
      src->enqueue(std::move(ptr), nullptr);
      return;
    }
    telemetry::timer::observe(mm_metrics.deserialization_time, t0);
    auto signed_size = static_cast<int64_t>(dref.payload_.size());
    mm_metrics.inbound_messages_size->observe(signed_size);
    // Intercept link messages. Forwarding actor proxies signalize linking
    // by sending link_atom/unlink_atom message with src == dest.
    if (auto view
        = make_const_typed_message_view<link_atom, strong_actor_ptr>(msg)) {
      const auto& ptr = get<1>(view);
      if (ptr != nullptr)
        static_cast<actor_proxy*>(ptr->get())->add_link(dst->get());
      else
        log::io::warning("received link message with invalid target");
      return;
    }
    if (auto view
        = make_const_typed_message_view<unlink_atom, strong_actor_ptr>(msg)) {
      const auto& ptr = get<1>(view);
      if (ptr != nullptr)
        static_cast<actor_proxy*>(ptr->get())->remove_link(dst->get());
      else
        log::io::debug("received unlink message with invalid target");
      return;
    }
    // Ship the message.
    guard.disable();
    dref.queue_->push(ctx, dref.msg_id_, std::move(dst),
                      make_mailbox_element(std::move(src), mid,
                                           std::move(msg)));
  }
};

} // namespace caf::io::basp
