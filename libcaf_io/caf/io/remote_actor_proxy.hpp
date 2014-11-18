/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

#ifndef CAF_IO_REMOTE_ACTOR_PROXY_HPP
#define CAF_IO_REMOTE_ACTOR_PROXY_HPP

#include "caf/extend.hpp"
#include "caf/actor_proxy.hpp"

#include "caf/mixin/memory_cached.hpp"

#include "caf/detail/single_reader_queue.hpp"

namespace caf {
namespace io {

/**
 * Implements a simple proxy forwarding all operations to a manager.
 */
class remote_actor_proxy : public actor_proxy {
 public:
  remote_actor_proxy(actor_id mid, node_id pinfo, actor parent);

  ~remote_actor_proxy();

  void enqueue(const actor_addr&, message_id,
               message, execution_unit*) override;

  bool link_impl(linking_operation op, const actor_addr& other) override;

  void local_link_to(const actor_addr& other) override;

  void local_unlink_from(const actor_addr& other) override;

  void kill_proxy(uint32_t reason) override;

 private:
  void forward_msg(const actor_addr& sender, message_id mid, message msg);
  actor m_manager;
};

} // namespace io
} // namespace caf

#endif // CAF_IO_REMOTE_ACTOR_PROXY_HPP
