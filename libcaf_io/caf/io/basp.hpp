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

#ifndef CAF_IO_BASP_HPP
#define CAF_IO_BASP_HPP

#include <map>
#include <set>
#include <string>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

#include "caf/node_id.hpp"
#include "caf/actor_addr.hpp"
#include "caf/serializer.hpp"
#include "caf/deserializer.hpp"
#include "caf/abstract_actor.hpp"
#include "caf/callback.hpp"
#include "caf/actor_namespace.hpp"

#include "caf/io/receive_policy.hpp"
#include "caf/io/abstract_broker.hpp"
#include "caf/io/system_messages.hpp"
#include "caf/io/connection_handle.hpp"

namespace caf {
namespace io {
namespace basp {

/// The current BASP version. Different BASP versions will not
/// be able to exchange messages.
constexpr uint64_t version = 2;

/// Storage type for raw bytes.
using buffer_type = std::vector<char>;

/// The header of a Binary Actor System Protocol (BASP) message.
/// A BASP header consists of a routing part, i.e., source and
/// destination, as well as an operation and operation data. Several
/// message types consist of only a header.
struct header {
  node_id source_node;
  node_id dest_node;
  actor_id source_actor;
  actor_id dest_actor;
  uint32_t payload_len;
  uint32_t operation;
  uint64_t operation_data;
};

/// @relates header
std::string to_string(const header& hdr);

/// @relates header
inline bool operator==(const header& lhs, const header& rhs) {
  return lhs.source_node == rhs.source_node
         && lhs.dest_node == rhs.dest_node
         && lhs.source_actor == rhs.source_actor
         && lhs.dest_actor == rhs.dest_actor
         && lhs.payload_len == rhs.payload_len
         && lhs.operation == rhs.operation
         && lhs.operation_data == rhs.operation_data;
}

/// @relates header
inline bool operator!=(const header& lhs, const header& rhs) {
  return !(lhs == rhs);
}

/// @relates header
void read_hdr(deserializer&, header& hdr);

/// @relates header
void write_hdr(serializer&, const header& hdr);

/// Denotes the state of a connection between to BASP nodes.
enum connection_state {
  /// Indicates that this node just started and is waiting for server handshake.
  await_server_handshake,
  /// Indicates that this node just accepted a connection, sent its
  /// handshake to the client, and is now waiting for the client's response.
  await_client_handshake,
  /// Indicates that a connection is established and this node is
  /// waiting for the next BASP header.
  await_header,
  /// Indicates that this node has received a header with non-zero payload
  /// and is waiting for the data.
  await_payload,
  /// Indicates that this connection no longer exists.
  close_connection
};

/// Stores routing information for a single broker participating as
/// BASP peer and provides both direct and indirect paths.
class routing_table {
public:
  explicit routing_table(abstract_broker* parent);

  virtual ~routing_table();

  /// Describes a routing path to a node.
  struct route {
    buffer_type& wr_buf;
    const node_id& next_hop;
    connection_handle hdl;
  };

  /// Describes a function object for erase operations that
  /// is called for each indirectly lost connection.
  using erase_callback = callback<const node_id&>;

  /// Returns a route to `target` or `none` on error.
  optional<route> lookup(const node_id& target);

  /// Returns the ID of the peer connected via `hdl` or
  /// `invalid_node_id` if `hdl` is unknown.
  node_id lookup_direct(const connection_handle& hdl) const;

  /// Returns the handle offering a direct connection to `nid` or
  /// `invalid_connection_handle` if no direct connection to `nid` exists.
  connection_handle lookup_direct(const node_id& nid) const;

  /// Flush output buffer for `r`.
  void flush(const route& r);

  /// Adds a new direct route to the table.
  /// @pre `hdl != invalid_connection_handle && nid != invalid_node_id`
  void add_direct(const connection_handle& hdl, const node_id& dest);

  /// Adds a new indirect route to the table.
  void add_indirect(const node_id& hop, const node_id& dest);

  /// Blacklist the route to `dest` via `hop`.
  void blacklist(const node_id& hop, const node_id& dest);

  /// Removes a direct connection and calls `cb` for any node
  /// that became unreachable as a result of this operation,
  /// including the node that is assigned as direct path for `hdl`.
  void erase_direct(const connection_handle& hdl, erase_callback& cb);

  /// Queries whether `dest` is reachable.
  bool reachable(const node_id& dest);

  /// Removes all direct and indirect routes to `dest` and calls
  /// `cb` for any node that became unreachable as a result of this
  /// operation, including `dest`.
  /// @returns the number of removed routes (direct and indirect)
  size_t erase(const node_id& dest, erase_callback& cb);

public:
  template <class Map, class Fallback>
  typename Map::mapped_type
  get_opt(const Map& m, const typename Map::key_type& k, Fallback&& x) const {
    auto i = m.find(k);
    if (i != m.end())
      return i->second;
    return std::forward<Fallback>(x);
  }

  using node_id_set = std::unordered_set<node_id>;

  using indirect_entries = std::unordered_map<node_id,      // dest
                                              node_id_set>; // hop

  abstract_broker* parent_;
  std::unordered_map<connection_handle, node_id> direct_by_hdl_;
  std::unordered_map<node_id, connection_handle> direct_by_nid_;
  indirect_entries indirect_;
  indirect_entries blacklist_;
};

/// Describes a protocol instance managing multiple connections.
class instance {
public:
  /// Provides a callback-based interface for certain BASP events.
  class callee {
  public:
    explicit callee(actor_namespace::backend& mgm);

    virtual ~callee();

    /// Called if a server handshake was received and
    /// the connection to `nid` is established.
    virtual void finalize_handshake(const node_id& nid, actor_id aid,
                                    const std::set<std::string>& sigs) = 0;

    /// Called whenever a direct connection was closed or a
    /// node became unrechable for other reasons *before*
    /// this node gets erased from the routing table.
    /// @warning The implementing class must not modify the
    ///          routing table from this callback.
    virtual void purge_state(const node_id& nid) = 0;

    virtual void proxy_announced(const node_id& nid, actor_id aid) = 0;

    virtual void kill_proxy(const node_id& nid, actor_id aid, uint32_t rsn) = 0;

    /// Called whenever a `dispatch_message` arrived for a local actor.
    virtual void deliver(const node_id& source_node, actor_id source_actor,
                         const node_id& dest_node, actor_id dest_actor,
                         message& msg, message_id mid) = 0;

    /// Returns the actor namespace associated to this BASP protocol instance.
    actor_namespace& get_namespace() {
      return namespace_;
    }

  public:
    actor_namespace namespace_;
  };

  /// Describes a function object responsible for writing
  /// the payload for a BASP message.
  using payload_writer = callback<serializer&>;

  /// Describes a callback function object for `remove_published_actor`.
  using removed_published_actor = callback<const actor_addr&, uint16_t>;

  instance(abstract_broker* parent, callee& lstnr);

  /// Handles received data and returns a config for receiving the
  /// next data or `none` if an error occured.
  connection_state handle(const new_data_msg& dm, header& hdr, bool is_payload);

  /// Handles connection shutdowns.
  void handle(const connection_closed_msg& msg);

  /// Handles failure or shutdown of a single node. This function purges
  /// all routes to `affected_node` from the routing table.
  void handle_node_shutdown(const node_id& affected_node);

  /// Returns a route to `target` or `none` on error.
  optional<routing_table::route> lookup(const node_id& target);

  /// Flushes the underlying buffer of `path`.
  void flush(const routing_table::route& path);

  /// Sends a BASP message and implicitly flushes the output buffer of `r`.
  /// This function will update `hdr.payload_len` if a payload was written.
  void write(const routing_table::route& r, header& hdr,
             payload_writer* writer = nullptr);

  /// Adds a new actor to the map of published actors.
  void add_published_actor(uint16_t port,
                           actor_addr published_actor,
                           std::set<std::string> published_interface);

  /// Removes the actor currently assigned to `port`.
  size_t remove_published_actor(uint16_t port,
                                removed_published_actor* cb = nullptr);

  /// Removes `whom` if it is still assigned to `port` or from all of its
  /// current ports if `port == 0`.
  size_t remove_published_actor(const actor_addr& whom, uint16_t port,
                                removed_published_actor* cb = nullptr);

  /// Returns `true` if a path to destination existed, `false` otherwise.
  bool dispatch(const actor_addr& sender, const actor_addr& receiver,
                message_id mid, const message& msg);

  /// Returns the actor namespace associated to this BASP protocol instance.
  actor_namespace& get_namespace() {
    return callee_.get_namespace();
  }

  /// Returns the routing table of this BASP instance.
  routing_table& tbl() {
    return tbl_;
  }

  /// Stores the address of a published actor along with its publicly
  /// visible messaging interface.
  using published_actor = std::pair<actor_addr, std::set<std::string>>;

  /// Maps ports to addresses and interfaces of published actors.
  using published_actor_map = std::unordered_map<uint16_t, published_actor>;

  /// Returns the current mapping of ports to addresses
  /// and interfaces of published actors.
  inline const published_actor_map& published_actors() const {
    return published_actors_;
  }

  /// Writes a header (build from the arguments)
  /// followed by its payload to `storage`.
  void write(buffer_type& storage,
             const node_id& source_node,
             const node_id& dest_node,
             actor_id source_actor,
             actor_id dest_actor,
             uint32_t* payload_len,
             uint32_t operation,
             uint64_t operation_data,
             payload_writer* writer = nullptr,
             bool suppress_auto_size_prefixing = false);

  /// Writes a header followed by its payload to `storage`.
  void write(buffer_type& storage, header& hdr,
             payload_writer* writer = nullptr,
             bool suppress_auto_size_prefixing = false);

  /// Writes the server handshake containing the information of the
  /// actor published at `port` to `buf`. If `port == none` or
  /// if no actor is published at this port then a standard handshake is
  /// written (e.g. used when establishing direct connections on-the-fly).
  void write_server_handshake(buffer_type& buf, optional<uint16_t> port);

  /// Writes a `dispatch_error` to `buf`.
  void write_dispatch_error(buffer_type& buf,
                            const node_id& source_node,
                            const node_id& dest_node,
                            uint64_t error_code,
                            const header& original_hdr,
                            const buffer_type& payload);

  /// Writes a `kill_proxy_instance` to `buf`.
  void write_kill_proxy_instance(buffer_type& buf,
                                 const node_id& dest_node,
                                 actor_id aid,
                                 uint32_t rsn);

  const node_id& this_node() const {
    return this_node_;
  }

private:
  routing_table tbl_;
  published_actor_map published_actors_;
  node_id this_node_;
  callee& callee_;
};

/// Deserialize a BASP message header from `source`.
void read_hdr(serializer& sink, header& hdr);

/// Serialize a BASP message header to `sink`.
void write_hdr(deserializer& source, const header& hdr);

/// Size of a BASP header in serialized form
constexpr size_t header_size =
  node_id::host_id_size * 2 + sizeof(uint32_t) * 2 +
  sizeof(actor_id) * 2 + sizeof(uint32_t) * 2 + sizeof(uint64_t);


inline bool valid(const node_id& val) {
  return val != invalid_node_id;
}

inline bool invalid(const node_id& val) {
  return ! valid(val);
}

template <class T>
inline bool zero(T val) {
  return val == 0;
}

template <class T>
inline bool nonzero(T aid) {
  return ! zero(aid);
}

/// Send from server, i.e., the node with a published actor, to client,
/// i.e., node that initiates a new connection using remote_actor().
///
/// Field          | Assignment
/// ---------------|----------------------------------------------------------
/// source_node    | ID of server
/// dest_node      | invalid
/// source_actor   | 0
/// dest_actor     | 0
/// payload_len    | size of actor id + interface definition
/// operation_data | BASP version of the server
constexpr uint32_t server_handshake = 0x00;

inline bool server_handshake_valid(const header& hdr) {
  return  valid(hdr.source_node)
       && invalid(hdr.dest_node)
       && zero(hdr.source_actor)
       && zero(hdr.dest_actor)
       && nonzero(hdr.payload_len)
       && nonzero(hdr.operation_data);
}

/// Send from client to server after it has successfully received the
/// server_handshake to establish the connection.
///
/// Field          | Assignment
/// ---------------|----------------------------------------------------------
/// source_node    | ID of client
/// dest_node      | ID of server
/// source_actor   | 0
/// dest_actor     | 0
/// payload_len    | 0
/// operation_data | 0
constexpr uint32_t client_handshake = 0x01;

inline bool client_handshake_valid(const header& hdr) {
  return  valid(hdr.source_node)
       && valid(hdr.dest_node)
       && hdr.source_node != hdr.dest_node
       && zero(hdr.source_actor)
       && zero(hdr.dest_actor)
       && zero(hdr.payload_len)
       && zero(hdr.operation_data);
}

/// Transmits a message from source_node:source_actor to
/// dest_node:dest_actor.
///
/// Field          | Assignment
/// ---------------|----------------------------------------------------------
/// source_node    | ID of sending node (invalid in case of anon_send)
/// dest_node      | ID of receiving node
/// source_actor   | ID of sending actor (invalid in case of anon_send)
/// dest_actor     | ID of receiving actor, must not be invalid
/// payload_len    | > 0
/// operation_data | message ID (0 for asynchronous messages)
///
/// Payload:
/// - the first 4 bytes are the size of the serialized message
/// - serialized message
/// - any number of node IDs: each forwarding node adds its
///   node ID to the payload, allowing the receiving node to backtrace
///   the full path of this message (also enables loop detection)
constexpr uint32_t dispatch_message = 0x02;

inline bool dispatch_message_valid(const header& hdr) {
  return  valid(hdr.dest_node)
       && nonzero(hdr.dest_actor)
       && nonzero(hdr.payload_len);
}

/// Informs the receiving node that the sending node has created a proxy
/// instance for one of its actors. Causes the receiving node to attach
/// a functor to the actor that triggers a kill_proxy_instance
/// message on termination.
///
/// Field          | Assignment
/// ---------------|----------------------------------------------------------
/// source_node    | ID of sending node
/// dest_node      | ID of receiving node
/// source_actor   | 0
/// dest_actor     | ID of monitored actor
/// payload_len    | 0
/// operation_data | 0
constexpr uint32_t announce_proxy_instance = 0x03;

inline bool announce_proxy_instance_valid(const header& hdr) {
  return  valid(hdr.source_node)
       && valid(hdr.dest_node)
       && hdr.source_node != hdr.dest_node
       && zero(hdr.source_actor)
       && nonzero(hdr.dest_actor)
       && zero(hdr.payload_len)
       && zero(hdr.operation_data);
}

/// Informs the receiving node that it has a proxy for an actor
/// that has been terminated.
///
/// Field          | Assignment
/// ---------------|----------------------------------------------------------
/// source_node    | ID of sending node
/// dest_node      | ID of receiving node
/// source_actor   | ID of monitored actor
/// dest_actor     | 0
/// payload_len    | 0
/// operation_data | exit reason (uint32)
constexpr uint32_t kill_proxy_instance = 0x04;

inline bool kill_proxy_instance_valid(const header& hdr) {
  return  valid(hdr.source_node)
       && valid(hdr.dest_node)
       && hdr.source_node != hdr.dest_node
       && nonzero(hdr.source_actor)
       && zero(hdr.dest_actor)
       && zero(hdr.payload_len)
       && nonzero(hdr.operation_data);
}

namespace error {

/// Indicates that a forwarding node had no route
/// to the destination.
static constexpr uint64_t no_route_to_destination = 0x01;

/// Indicates that a forwarding node detected
/// a loop in the forwarding path.
static constexpr uint64_t loop_detected = 0x02;

} // namespace error

/// Informs the receiving node that a message it sent
/// did not reach its destination.
///
/// Field          | Assignment
/// ---------------|----------------------------------------------------------
/// source_node    | ID of sending node
/// dest_node      | ID of receiving node
/// source_actor   | 0
/// dest_actor     | 0
/// payload_len    | > 0 (size of message object and forwarding nodes)
/// operation_data | error code (see `basp::error`)
constexpr uint32_t dispatch_error = 0x05;

inline bool dispatch_error_valid(const header& hdr) {
  return  valid(hdr.source_node)
       && valid(hdr.dest_node)
       && hdr.source_node != hdr.dest_node
       && zero(hdr.source_actor)
       && zero(hdr.dest_actor)
       && nonzero(hdr.payload_len)
       && nonzero(hdr.operation_data);
}

/// Checks whether given header contains a handshake.
inline bool is_handshake(const header& hdr) {
  return hdr.operation == server_handshake || hdr.operation == client_handshake;
}

/// Checks whether given header is valid.
inline bool valid(const header& hdr) {
  switch (hdr.operation) {
    default:
      return false; // invalid operation field
    case server_handshake:
      return server_handshake_valid(hdr);
    case client_handshake:
      return client_handshake_valid(hdr);
    case dispatch_message:
      return dispatch_message_valid(hdr);
    case announce_proxy_instance:
      return announce_proxy_instance_valid(hdr);
    case kill_proxy_instance:
      return kill_proxy_instance_valid(hdr);
  }
}

} // namespace basp
} // namespace io
} // namespace caf

#endif // CAF_IO_BASP_HPP
