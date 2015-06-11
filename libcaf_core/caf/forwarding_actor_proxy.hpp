/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_FORWARDING_ACTOR_PROXY_HPP
#define CAF_FORWARDING_ACTOR_PROXY_HPP

#include "caf/actor.hpp"
#include "caf/actor_proxy.hpp"

#include "caf/detail/shared_spinlock.hpp"

namespace caf {

/// Implements a simple proxy forwarding all operations to a manager.
class forwarding_actor_proxy : public actor_proxy {
public:
  forwarding_actor_proxy(actor_id mid, node_id pinfo, actor parent);

  ~forwarding_actor_proxy();

  void enqueue(const actor_addr&, message_id,
               message, execution_unit*) override;

  bool link_impl(linking_operation op, const actor_addr& other) override;

  void local_link_to(const actor_addr& other) override;

  void local_unlink_from(const actor_addr& other) override;

  void kill_proxy(uint32_t reason) override;

  actor manager() const;

  void manager(actor new_manager);

private:
  void forward_msg(const actor_addr& sender, message_id mid, message msg);

  mutable detail::shared_spinlock manager_mtx_;
  actor manager_;
};

} // namespace caf

#endif // CAF_FORWARDING_ACTOR_PROXY_HPP
