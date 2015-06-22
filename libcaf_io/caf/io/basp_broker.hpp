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

#include "caf/actor_namespace.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/forwarding_actor_proxy.hpp"

#include "caf/io/basp.hpp"
#include "caf/io/broker.hpp"

namespace caf {
namespace io {

/// A broker implementation for the Binary Actor System Protocol (BASP).
class basp_broker : public broker, public actor_namespace::backend {
public:
  basp_broker(middleman& parent_ref);

  ~basp_broker();

  behavior make_behavior() override;

  void add_published_actor(accept_handle hdl, const abstract_actor_ptr& whom,
                           uint16_t port);

  bool remove_published_actor(const abstract_actor_ptr& whom);

  bool remove_published_actor(const abstract_actor_ptr& whom, uint16_t port);

  actor_proxy_ptr make_proxy(const node_id&, actor_id) override;

  class payload_writer {
 public:
    payload_writer() = default;
    payload_writer(const payload_writer&) = default;
    payload_writer& operator=(const payload_writer&) = default;
    virtual ~payload_writer();
    virtual void write(binary_serializer&) = 0;
  };

  void dispatch(connection_handle hdl, uint32_t operation,
                const node_id& src_node, actor_id src_actor,
                const node_id& dest_node, actor_id dest_actor,
                uint64_t op_data = 0, payload_writer* writer = nullptr);

  node_id dispatch(uint32_t operation, const node_id& src_node,
                   actor_id src_actor, const node_id& dest_node,
                   actor_id dest_actor, uint64_t op_data = 0,
                   payload_writer* writer = nullptr);

  // dispatches a message from a local actor to a remote node
  node_id dispatch(const actor_addr& from, const actor_addr& to,
                   message_id mid, const message& msg);

  struct client_handshake_data {
    int64_t request_id;
    actor client;
    std::set<std::string> expected_ifs;
  };

  inline actor_namespace& get_namespace() {
    return namespace_;
  }

protected:
  void on_exit() override;

private:
  void erase_proxy(const node_id& nid, actor_id aid);

  // dispatches a message from a remote node to a local actor
  void local_dispatch(const basp::header& msg, message&& payload);

  enum connection_state {
    // client just started, await handshake from server
    await_server_handshake,
    // server accepted new connection and sent handshake, await response
    await_client_handshake,
    // connection established, read series of broker messages
    await_header,
    // currently waiting for payload of a received message
    await_payload,
    // connection is going to be shut down because of an error
    close_connection
  };

  struct connection_context {
    connection_state state;
    connection_handle hdl;
    node_id remote_id;
    optional<client_handshake_data> handshake_data;
    basp::header hdr;
    // keep a reference to the published actor of
    // the remote node to prevent this particular
    // proxy instance from expiring; this prevents
    // a bug where re-using an "old" connection via
    // remote_actor() could return an expired proxy
    actor published_actor;
  };

  void read(binary_deserializer& bs, basp::header& msg);

  void write(binary_serializer& bs, const basp::header& msg);

  void send_kill_proxy_instance(const node_id& nid, actor_id aid,
                                uint32_t reason);

  connection_state handle_basp_header(connection_context& ctx,
                                      const buffer_type* payload = nullptr);

  optional<skip_message_t> add_monitor(connection_context& ctx, actor_id aid);

  optional<skip_message_t> kill_proxy(connection_context& ctx, actor_id aid,
                                      std::uint32_t reason);

  void new_data(connection_context& ctx, buffer_type& buf);

  void init_handshake_as_client(connection_context& ctx);

  void init_handshake_as_server(connection_context& ctx,
                                actor_addr published_actor);

  void serialize_msg(const actor_addr& sender, message_id mid,
                     const message& msg, buffer_type& wr_buf);

  bool try_set_default_route(const node_id& nid, connection_handle hdl);

  void add_route(const node_id& nid, connection_handle hdl);

  struct connection_info {
    connection_handle hdl;
    node_id node;
    inline bool invalid() const {
      return hdl.invalid();
    }
    inline bool operator==(const connection_info& other) const {
      return hdl == other.hdl && node == other.node;
    }
    inline bool operator<(const connection_info& other) const {
      return hdl < other.hdl;
    }
  };

  connection_info get_route(const node_id& dest);

  // node_id of the target, sender of the request, and original request ID
  using pending_request = std::tuple<node_id, actor_addr, message_id>;
  using pending_requests = std::vector<pending_request>;
  using pending_request_iter = pending_requests::iterator;

  void fail_pending_requests(pending_request_iter first,
                             pending_request_iter last,
                             uint32_t reason);

  // fails all pending requests (total network failure or shutdown)
  void fail_pending_requests(uint32_t reason);

  // fails all pending requests from `addr` (disconnect from node)
  void fail_pending_requests(const node_id& addr, uint32_t reason);

  struct connection_info_less {
    inline bool operator()(const connection_info& lhs,
                           const connection_info& rhs) const {
      return lhs.hdl < rhs.hdl;
    }
    inline bool operator()(const connection_info& lhs,
                           const connection_handle& rhs) const {
      return lhs.hdl < rhs;
    }
  };

  using blacklist_entry = std::pair<node_id, connection_handle>;

  // (default route, [alternative routes])
  using routing_table_entry = std::pair<connection_info,
                                        std::set<connection_info>>;

  struct blacklist_less {
    inline bool operator()(const blacklist_entry& lhs,
                           const blacklist_entry& rhs) const {
      if (lhs.first < rhs.first) {
        return lhs.second < rhs.second;
      }
      return false;
    }
  };

  // dest => hops
  using routing_table = std::map<node_id, routing_table_entry>;

  actor_namespace namespace_; // manages proxies
  std::map<connection_handle, connection_context> ctx_;
  std::map<accept_handle, std::pair<abstract_actor_ptr, uint16_t>> acceptors_;
  std::map<uint16_t, accept_handle> open_ports_;
  routing_table routes_; // stores non-direct routes
  std::set<blacklist_entry, blacklist_less> blacklist_; // stores invalidated
                                                         // routes
  // a simple "bag" for our pending requests
  std::vector<pending_request> pending_requests_;

  // needed to keep track to which node we are talking to at the moment
  connection_context* current_context_;

  // cache some UTIs to make serialization a bit faster
  const uniform_type_info* meta_msg_;
  const uniform_type_info* meta_id_type_;
};

} // namespace io
} // namespace caf

#endif // CAF_IO_BASP_BROKER_HPP
