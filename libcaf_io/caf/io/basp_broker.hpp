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
#include "caf/io/visitors.hpp"
#include "caf/io/typed_broker.hpp"

#include "caf/io/basp/endpoint_context.hpp"

namespace caf {
namespace io {

struct basp_broker_state : proxy_registry::backend, basp::instance::callee {
  basp_broker_state(broker* selfptr);

  ~basp_broker_state() override;

  // inherited from proxy_registry::backend
  strong_actor_ptr make_proxy(node_id nid, actor_id aid) override;

  // inherited from proxy_registry::backend
  execution_unit* registry_context() override;

  // inherited from basp::instance::callee
  void finalize_handshake(const node_id& nid, actor_id aid,
                          std::set<std::string>& sigs) override;

  // inherited from basp::instance::callee
  void purge_state(const node_id& nid) override;

  // inherited from basp::instance::callee
  void proxy_announced(const node_id& nid, actor_id aid) override;

  // inherited from basp::instance::callee
  void deliver(const node_id& src_nid, actor_id src_aid,
               actor_id dest_aid, message_id mid,
               std::vector<strong_actor_ptr>& stages, message& msg) override;

  // inherited from basp::instance::callee
  void deliver(const node_id& src_nid, actor_id src_aid,
               atom_value dest_name, message_id mid,
               std::vector<strong_actor_ptr>& stages, message& msg) override;

  // called from both overriden functions
  void deliver(const node_id& src_nid, actor_id src_aid,
               strong_actor_ptr dest, message_id mid,
               std::vector<strong_actor_ptr>& stages, message& msg);

  // performs bookkeeping such as managing `spawn_servers`
  void learned_new_node(const node_id& nid);

  // inherited from basp::instance::callee
  void learned_new_node_directly(const node_id& nid,
                                 bool was_indirectly_before) override;

  // inherited from basp::instance::callee
  void learned_new_node_indirectly(const node_id& nid) override;

  // inherited from basp::instance::callee
  uint16_t next_sequence_number(connection_handle hdl) override;

  // inherited from basp::instance::callee
  uint16_t next_sequence_number(datagram_handle hdl) override;

  // inherited from basp::instance::callee
  void add_pending(execution_unit* ctx, basp::endpoint_context& ep,
                   uint16_t seq, basp::header hdr,
                   std::vector<char> payload) override;

  // inherited from basp::instance::callee
  bool deliver_pending(execution_unit* ctx, basp::endpoint_context& ep,
                       bool force) override;

  // inherited from basp::instance::callee
  void drop_pending(basp::endpoint_context& ep, uint16_t seq) override;

  // inherited from basp::instance::callee
  buffer_type& get_buffer(endpoint_handle hdl) override;

  // inherited from basp::instance::callee
  buffer_type& get_buffer(datagram_handle hdl) override;

  // inherited from basp::instance::callee
  buffer_type& get_buffer(connection_handle hdl) override;

  // inherited from basp::instance::callee
  buffer_type pop_datagram_buffer(datagram_handle hdl) override;

  // inherited from basp::instance::callee
  void flush(endpoint_handle hdl) override;

  // inherited from basp::instance::callee
  void flush(datagram_handle hdl) override;

  // inherited from basp::instance::callee
  void flush(connection_handle hdl) override;

  void handle_heartbeat(const node_id&) override {
    // nop
  }

  /// Sets `this_context` by either creating or accessing state for `hdl`.
  void set_context(connection_handle hdl);
  void set_context(datagram_handle hdl);

  /// Cleans up any state for `hdl`.
  void cleanup(connection_handle hdl);
  void cleanup(datagram_handle hdl);

  // pointer to ourselves
  broker* self;

  // protocol instance of BASP
  basp::instance instance;

  using ctx_tcp_map = std::unordered_map<connection_handle,
                                         basp::endpoint_context>;
  using ctx_udp_map = std::unordered_map<datagram_handle,
                                         basp::endpoint_context>;

  // keeps context information for all open connections
  ctx_tcp_map ctx_tcp;
  ctx_udp_map ctx_udp;

  // points to the current context for callbacks such as `make_proxy`
  basp::endpoint_context* this_context = nullptr;

  // stores handles to spawn servers for other nodes; these servers
  // are spawned whenever the broker learns a new node ID and try to
  // get a 'SpawnServ' instance on the remote side
  std::unordered_map<node_id, actor> spawn_servers;

  /// Configures whether BASP automatically open new connections to optimize
  /// routing paths by forming a mesh between all nodes.
  bool automatic_connections = false;

  /// Configures whether BASP allows TCP connections.
  bool allow_tcp = true;

  /// Configures whether BASP allows UDP connections.
  bool allow_udp = false;

  // reusable send buffers for UDP communication
  const size_t max_buffers;
  std::stack<buffer_type> cached_buffers;

  // maximum queue size for pending messages of endpoints with ordering
  const size_t max_pending_messages;

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

  // sends a kill_proxy message to a remote node
  void send_kill_proxy_instance(const node_id& nid, actor_id aid, error err);

  // sends kill_proxy_instance message to all nodes monitoring the terminated
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

