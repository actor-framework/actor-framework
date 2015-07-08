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

#ifndef CAF_IO_BASP_BROKER_HPP
#define CAF_IO_BASP_BROKER_HPP

#include <map>
#include <set>
#include <string>
#include <future>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "caf/actor_namespace.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/forwarding_actor_proxy.hpp"

#include "caf/experimental/stateful_actor.hpp"

#include "caf/io/basp.hpp"
#include "caf/io/broker.hpp"

namespace caf {
namespace io {

struct basp_broker_state : actor_namespace::backend, basp::instance::callee {
  basp_broker_state(broker* self);

  // inherited from actor_namespace::backend
  actor_proxy_ptr make_proxy(const node_id& nid, actor_id aid) override;

  void erase_proxy(const node_id& nid, actor_id aid);

  // inherited from basp::instance::listener
  void finalize_handshake(const node_id& nid, actor_id aid,
                          const std::set<std::string>& sigs) override;

  // inherited from basp::instance::listener
  void purge_state(const node_id& id) override;

  // inherited from basp::instance::listener
  void proxy_announced(const node_id& nid, actor_id aid) override;

  // inherited from basp::instance::listener
  void kill_proxy(const node_id& nid, actor_id aid, uint32_t rsn) override;

  void deliver(const node_id& source_node, actor_id source_actor,
               const node_id& dest_node, actor_id dest_actor,
               message& msg, message_id mid) override;

  struct connection_context {
    basp::connection_state cstate;
    basp::header hdr;
    connection_handle hdl;
    node_id id;
    uint16_t remote_port;
    optional<response_promise> callback;
    std::set<std::string> expected_sigs;
  };

  void set_context(connection_handle hdl);

  bool erase_context(connection_handle hdl);

  // pointer to ourselves
  broker* self;

  // protocol instance of BASP
  basp::instance instance;

  // keeps context information for all open connections
  std::unordered_map<connection_handle, connection_context> ctx;

  // points to the current context for callbacks such as `make_proxy`
  connection_context* this_context = nullptr;

  // stores all published actors we know from other nodes, this primarily
  // keeps the associated proxies alive to work around subtle bugs
  std::unordered_map<node_id, std::pair<uint16_t, actor_addr>> known_remotes;

  const node_id& this_node() const {
    return instance.this_node();
  }
};

/// A broker implementation for the Binary Actor System Protocol (BASP).
class basp_broker : public caf::experimental::stateful_actor<basp_broker_state,
                                                             broker> {
public:
  basp_broker(middleman& mm);
  behavior make_behavior() override;
};

} // namespace io
} // namespace caf

#endif // CAF_IO_BASP_BROKER_HPP
