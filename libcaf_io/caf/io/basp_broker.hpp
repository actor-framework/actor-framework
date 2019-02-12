/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
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

#include <map>
#include <set>
#include <stack>
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
  void set_last_hop(node_id* ptr) override;

  // inherited from basp::instance::callee
  void finalize_handshake(const node_id& nid, actor_id aid,
                          std::set<std::string>& sigs) override;

  // inherited from basp::instance::callee
  void purge_state(const node_id& nid) override;

  // inherited from basp::instance::callee
  void proxy_announced(const node_id& nid, actor_id aid) override;

  // performs bookkeeping such as managing `spawn_servers`
  void learned_new_node(const node_id& nid);

  // inherited from basp::instance::callee
  void learned_new_node_directly(const node_id& nid,
                                 bool was_indirectly_before) override;

  // inherited from basp::instance::callee
  void learned_new_node_indirectly(const node_id& nid) override;

  // inherited from basp::instance::callee
  buffer_type& get_buffer(connection_handle hdl) override;

  // inherited from basp::instance::callee
  void flush(connection_handle hdl) override;

  // inherited from basp::instance::callee
  void handle_heartbeat() override;

  // inherited from basp::instance::callee
  execution_unit* current_execution_unit() override;

  /// Sets `this_context` by either creating or accessing state for `hdl`.
  void set_context(connection_handle hdl);

  /// Cleans up any state for `hdl`.
  void cleanup(connection_handle hdl);

  // pointer to ourselves
  broker* self;

  // protocol instance of BASP
  basp::instance instance;

  using ctx_map = std::unordered_map<connection_handle, basp::endpoint_context>;

  // keeps context information for all open connections
  ctx_map ctx;

  // points to the current context for callbacks such as `make_proxy`
  basp::endpoint_context* this_context = nullptr;

  // stores handles to spawn servers for other nodes; these servers
  // are spawned whenever the broker learns a new node ID and try to
  // get a 'SpawnServ' instance on the remote side
  std::unordered_map<node_id, actor> spawn_servers;

  /// Configures whether BASP automatically open new connections to optimize
  /// routing paths by forming a mesh between all nodes.
  bool automatic_connections = false;

  // timeout for delivery of pending messages of endpoints with ordering
  const std::chrono::milliseconds pending_to = std::chrono::milliseconds(100);

  // returns the node identifier of the underlying BASP instance
  const node_id& this_node() const {
    return instance.this_node();
  }

  using monitored_actor_map =
    std::unordered_map<actor_addr, std::unordered_set<node_id>>;

  // keeps a list of nodes that monitor a particular local actor
  monitored_actor_map monitored_actors;

  // sends a basp::down_message message to a remote node
  void send_basp_down_message(const node_id& nid, actor_id aid, error err);

  // sends basp::down_message to all nodes monitoring the terminated
  // actor
  void handle_down_msg(down_msg&);

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

