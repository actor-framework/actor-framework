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
#include "caf/detail/local_group_module.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/fwd.hpp"
#include "caf/stateful_actor.hpp"

namespace caf::detail {

/// Represents a group that runs on a different CAF node.
class group_tunnel : public local_group_module::impl {
public:
  using super = local_group_module::impl;

  group_tunnel(group_module_ptr mod, std::string id,
               actor upstream_intermediary);

  ~group_tunnel() override;

  bool subscribe(strong_actor_ptr who) override;

  void unsubscribe(const actor_control_block* who) override;

  // Locally enqueued message, forwarded via worker_.
  void enqueue(strong_actor_ptr sender, message_id mid, message content,
               execution_unit* host) override;

  void stop() override;

  // Messages received from the upstream group, forwarded to local subscribers.
  void upstream_enqueue(strong_actor_ptr sender, message_id mid,
                        message content, execution_unit* host);

  auto worker() const noexcept {
    return worker_;
  }

private:
  actor worker_;
};

using group_tunnel_ptr = intrusive_ptr<group_tunnel>;

} // namespace caf::detail
