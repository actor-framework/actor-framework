/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
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

#include "caf/abstract_group.hpp"
#include "caf/actor.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/local_group_module.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/fwd.hpp"
#include "caf/stateful_actor.hpp"

#include <tuple>

namespace caf::detail {

/// Represents a group that runs on a different CAF node.
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
  void enqueue(strong_actor_ptr sender, message_id mid, message content,
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
