// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor_proxy.hpp"
#include "caf/net/endpoint_manager.hpp"

namespace caf::net {

/// Implements a simple proxy forwarding all operations to a manager.
class actor_proxy_impl : public actor_proxy {
public:
  using super = actor_proxy;

  actor_proxy_impl(actor_config& cfg, endpoint_manager_ptr dst);

  ~actor_proxy_impl() override;

  void enqueue(mailbox_element_ptr what, execution_unit* context) override;

  void kill_proxy(execution_unit* ctx, error rsn) override;

private:
  endpoint_manager_ptr dst_;
};

} // namespace caf::net
