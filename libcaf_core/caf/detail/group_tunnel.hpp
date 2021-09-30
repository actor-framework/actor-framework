// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/abstract_group.hpp"
#include "caf/actor.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/local_group_module.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/fwd.hpp"
#include "caf/stateful_actor.hpp"

#include <tuple>

namespace caf::detail {

/// Represents a group that runs on a remote CAF node.
class CAF_CORE_EXPORT group_tunnel : public local_group_module::impl {
public:
  using super = local_group_module::impl;

  using cached_message = std::tuple<strong_actor_ptr, message_id, message>;

  using cached_message_list = std::vector<cached_message>;

  // Creates a connected tunnel.
  group_tunnel(group_module_ptr mod, std::string id,
               actor upstream_intermediary);

  // Creates an unconnected tunnel that caches incoming messages until it
  // becomes connected to the upstream intermediary.
  group_tunnel(group_module_ptr mod, std::string id, node_id origin);

  ~group_tunnel() override;

  bool subscribe(strong_actor_ptr who) override;

  void unsubscribe(const actor_control_block* who) override;

  // Locally enqueued message, forwarded via worker_.
  bool enqueue(strong_actor_ptr sender, message_id mid, message content,
               execution_unit* host) override;

  void stop() override;

  std::string stringify() const override;

  // Messages received from the upstream group, forwarded to local subscribers.
  void upstream_enqueue(strong_actor_ptr sender, message_id mid,
                        message content, execution_unit* host);

  bool connect(actor upstream_intermediary);

  bool connected() const noexcept;

  actor worker() const noexcept;

private:
  actor worker_;
  cached_message_list cached_messages_;
};

using group_tunnel_ptr = intrusive_ptr<group_tunnel>;

} // namespace caf::detail
