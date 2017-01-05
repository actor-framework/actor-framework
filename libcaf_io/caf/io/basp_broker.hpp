/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#include "caf/stateful_actor.hpp"
#include "caf/proxy_registry.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/forwarding_actor_proxy.hpp"

#include "caf/io/basp/all.hpp"
#include "caf/io/broker.hpp"
#include "caf/io/typed_broker.hpp"

namespace caf {
namespace io {

struct basp_broker_state : proxy_registry::backend, basp::instance::callee {
  basp_broker_state(broker* selfptr);

  ~basp_broker_state() override;

  // inherited from proxy_registry::backend
  strong_actor_ptr make_proxy(node_id nid, actor_id aid) override;

  // inherited from proxy_registry::backend
  execution_unit* registry_context() override;

  // inherited from basp::instance::listener
  void finalize_handshake(const node_id& nid, actor_id aid,
                          std::set<std::string>& sigs) override;

  // inherited from basp::instance::listener
  void purge_state(const node_id& nid) override;

  // inherited from basp::instance::listener
  void proxy_announced(const node_id& nid, actor_id aid) override;

  // inherited from basp::instance::listener
  void kill_proxy(const node_id& nid, actor_id aid, const error& rsn) override;

  // inherited from basp::instance::listener
  void deliver(const node_id& src_nid, actor_id src_aid,
               actor_id dest_aid, message_id mid,
               std::vector<strong_actor_ptr>& stages, message& msg) override;

  // inherited from basp::instance::listener
  void deliver(const node_id& src_nid, actor_id src_aid,
               atom_value dest_name, message_id mid,
               std::vector<strong_actor_ptr>& stages, message& msg) override;

  // called from both overriden functions
  void deliver(const node_id& src_nid, actor_id src_aid,
               strong_actor_ptr dest, message_id mid,
               std::vector<strong_actor_ptr>& stages, message& msg);

  // performs bookkeeping such as managing `spawn_servers`
  void learned_new_node(const node_id& nid);

  // inherited from basp::instance::listener
  void learned_new_node_directly(const node_id& nid,
                                 bool was_indirectly_before) override;

  // inherited from basp::instance::listener
  void learned_new_node_indirectly(const node_id& nid) override;

  void handle_heartbeat(const node_id&) override {
    // nop
  }

  // stores meta information for open connections
  struct connection_context {
    // denotes what message we expect from the remote node next
    basp::connection_state cstate;
    // our currently processed BASP header
    basp::header hdr;
    // the connection handle for I/O operations
    connection_handle hdl;
    // network-agnostic node identifier
    node_id id;
    // connected port
    uint16_t remote_port;
    // pending operations to be performed after handhsake completed
    optional<response_promise> callback;
  };

  void set_context(connection_handle hdl);

  // pointer to ourselves
  broker* self;

  // protocol instance of BASP
  basp::instance instance;

  // keeps context information for all open connections
  std::unordered_map<connection_handle, connection_context> ctx;

  // points to the current context for callbacks such as `make_proxy`
  connection_context* this_context = nullptr;

  // stores handles to spawn servers for other nodes; these servers
  // are spawned whenever the broker learns a new node ID and try to
  // get a 'SpawnServ' instance on the remote side
  std::unordered_map<node_id, actor> spawn_servers;

  // can be enabled by the user to let CAF automatically try
  // to establish new connections at runtime to optimize
  // routing paths by forming a mesh between all nodes
  bool enable_automatic_connections = false;

  // returns the node identifier of the underlying BASP instance
  const node_id& this_node() const {
    return instance.this_node();
  }

  static const char* name;
};

/// A broker implementation for the Binary Actor System Protocol (BASP).
class basp_broker : public stateful_actor<basp_broker_state, broker> {
public:
  using super = stateful_actor<basp_broker_state, broker>;

  explicit basp_broker(actor_config& cfg);

  behavior make_behavior() override;
  proxy_registry* proxy_registry_ptr() override;
  resume_result resume(execution_unit*, size_t) override;
};

} // namespace io
} // namespace caf

#endif // CAF_IO_BASP_BROKER_HPP
