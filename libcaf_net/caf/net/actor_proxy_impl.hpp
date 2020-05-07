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
