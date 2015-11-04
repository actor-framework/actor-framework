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

#include "caf/fwd.hpp"
#include "caf/node_id.hpp"
#include "caf/callback.hpp"
#include "caf/actor_addr.hpp"
#include "caf/serializer.hpp"
#include "caf/deserializer.hpp"
#include "caf/abstract_actor.hpp"
#include "caf/actor_namespace.hpp"

#include "caf/io/middleman.hpp"
#include "caf/io/receive_policy.hpp"
#include "caf/io/abstract_broker.hpp"
#include "caf/io/system_messages.hpp"
#include "caf/io/connection_handle.hpp"

namespace caf {
namespace io {
namespace basp {

/// @defgroup BASP Binary Actor Sytem Protocol
///
/// # Protocol Overview
///
/// The "Binary Actor Sytem Protocol" (BASP) is **not** a network protocol.
/// It is a specification for the "Remote Method Invocation" (RMI) interface
/// used by distributed instances of CAF. The purpose of BASP is unify the
/// structure of RMI calls in order to simplify processing and implementation.
/// Hence, BASP is independent of any underlying network technology,
/// and assumes a reliable communication channel.
///
///
/// The RMI interface of CAF enables network-transparent monitoring and linking
/// as well as global message dispatching to actors running on different nodes.
///
/// ![](basp_overview.png)
///
/// The figure above illustrates the phyiscal as well as the logical view
/// of a distributed CAF application. Note that the actors used for the
/// BASP communication ("BASP Brokers") are not part of the logical system
/// view and are in fact not visible to other actors. A BASP Broker creates
/// proxy actors that represent actors running on different nodes. It is worth
/// mentioning that two instances of CAF running on the same physical machine
/// are considered two different nodes in BASP.
///
/// BASP has two objectives:
///
/// - **Forward messages sent to a proxy to the actor it represents**
///
///   Whenever a proxy instance receives a message, it forwards this message to
///   its parent (a BASP Broker). This message is then serialized and forwarded
///   over the network. If no direct connection between the node sending the
///   message and the node receiving it exists, intermediate BASP Brokers will
///   forward it until it the message reaches its destination.
///
/// - **Synchronize the state of an actor with all of its proxies**.
///
///   Whenever a node learns the address of a remotely running actor, it
///   creates  Ma local proxy instance representing this actor and sends an
///   `announce_proxy_instance` to the node hosting the actor. Whenever an actor
///   terminates, the hosting node sends `kill_proxy_instance` messages to all
///   nodes that have a proxy for this actor. This enables network-transparent
///   actor monitoring. There are two possible ways addresses can be learned:
///
///   + A client connects to a remotely running (published) actor via
///     `remote_actor`. In this case, the `server_handshake` will contain the
///     address of the published actor.
///
///   + Receiving `dispatch_message`. Whenever an actor message arrives,
///     it usually contains the address of the sender. Further, the message
///     itself can contain addresses to other actors that the BASP Broker
///     will get aware of while deserializing the message object
///     from the payload.
///
/// # Node IDs
///
/// The ID of a node consists of a 120 bit hash and the process ID. Note that
/// we use "node" as a synonym for "CAF instance". The hash is generated from
/// "low-level" characteristics of a machine such as the UUID of the root
/// file system and available MAC addresses. The only purpose of the node ID
/// is to generate a network-wide unique identifier. By adding the process ID,
/// CAF disambiguates multiple instances running on the same phyisical machine.
///
/// # Header Format
///
/// ![](basp_header.png)
///
/// - **Operation ID**: 4 bytes.
///
///   This field indicates what BASP function this datagram represents.
///   The value is an `uint32_t` representation of `message_type`.
///
/// - **Payload Length**: 4 bytes.
///
///   The length of the data following this header as `uint32_t`,
///   measured in bytes.
///
/// - **Operation Data**: 8 bytes.
///
///   This field contains.
///
/// - **Source Node ID**: 18 bytes.
///
///   The address of the source node. See [Node IDs](# Node IDs).
///
/// - **Destination Node ID**: 18 bytes.
///
///   The address of the destination node. See [Node IDs](# Node IDs).
///   Upon receiving this datagram, a BASP Broker compares this node ID
///   to its own ID. On a mismatch, it selects the next hop and forwards
///   this datagram unchanged.
///
/// - **Source Actor ID**: 4 bytes.
///
///   This field contains the ID of the sending actor or 0 for anonymously
///   sent messages. The *full address* of an actor is the combination of
///   the node ID and the actor ID. Thus, every actor can be unambigiously
///   identified by combining these two IDs.
///
/// - **Destination Actor ID**: 4 bytes.
///
///   This field contains the ID of the receiving actor or 0 for BASP
///   functions that do not require
///
/// # Example
///
/// The following diagram models a distributed application
/// with three nodes. The pseudo code for the application can be found
/// in the three grey boxes, while the resulting BASP messaging
/// is shown in UML sequence diagram notation. More details about
/// individual BASP message types can be found in the documentation
/// of {@link message_type} below.
///
/// ![](basp_sequence.png)

/// @addtogroup BASP
/// @{

/// The current BASP version. Different BASP versions will not
/// be able to exchange messages.
constexpr uint64_t version = 1;

/// Storage type for raw bytes.
using buffer_type = std::vector<char>;

/// Describes the first header field of a BASP message and determines the
/// interpretation of the other header fields.
enum class message_type : uint32_t {
  /// Send from server, i.e., the node with a published actor, to client,
  /// i.e., node that initiates a new connection using remote_actor().
  ///
  /// ![](server_handshake.png)
  server_handshake = 0x00,

  /// Send from client to server after it has successfully received the
  /// server_handshake to establish the connection.
  ///
  /// ![](client_handshake.png)
  client_handshake = 0x01,

  /// Transmits a message from `source_node:source_actor` to
  /// `dest_node:dest_actor`.
  ///
  /// ![](dispatch_message.png)
  dispatch_message = 0x02,

  /// Informs the receiving node that the sending node has created a proxy
  /// instance for one of its actors. Causes the receiving node to attach
  /// a functor to the actor that triggers a kill_proxy_instance
  /// message on termination.
  ///
  /// ![](announce_proxy_instance.png)
  announce_proxy_instance = 0x03,

  /// Informs the receiving node that it has a proxy for an actor
  /// that has been terminated.
  ///
  /// ![](kill_proxy_instance.png)
  kill_proxy_instance = 0x04
};

/// @relates message_type
std::string to_string(message_type);

/// The header of a Binary Actor System Protocol (BASP) message.
/// A BASP header consists of a routing part, i.e., source and
/// destination, as well as an operation and operation data. Several
/// message types consist of only a header.
struct header {
  message_type operation;
  uint32_t payload_len;
  uint64_t operation_data;
  node_id source_node;
  node_id dest_node;
  actor_id source_actor;
  actor_id dest_actor;
};

/// @relates header
std::string to_string(const header& hdr);

/// @relates header
void read_hdr(deserializer& source, header& hdr);

/// @relates header
void write_hdr(serializer& sink, const header& hdr);

/// @relates header
bool operator==(const header& lhs, const header& rhs);

/// @relates header
inline bool operator!=(const header& lhs, const header& rhs) {
  return !(lhs == rhs);
}

/// Checks whether given BASP header is valid.
/// @relates header
bool valid(const header& hdr);

/// Deserialize a BASP message header from `source`.
void read_hdr(serializer& sink, header& hdr);

/// Serialize a BASP message header to `sink`.
void write_hdr(deserializer& source, const header& hdr);

/// Size of a BASP header in serialized form
constexpr size_t header_size =
  node_id::host_id_size * 2 + sizeof(uint32_t) * 2 +
  sizeof(actor_id) * 2 + sizeof(uint32_t) * 2 + sizeof(uint64_t);

/// Describes an error during forwarding of BASP messages.
enum class error : uint64_t {
  /// Indicates that a forwarding node had no route
  /// to the destination.
  no_route_to_destination = 0x01,

  /// Indicates that a forwarding node detected
  /// a loop in the forwarding path.
  loop_detected = 0x02
};

/// Denotes the state of a connection between to BASP nodes.
enum connection_state {
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
  maybe<route> lookup(const node_id& target);

  /// Returns the ID of the peer connected via `hdl` or
  /// `invalid_node_id` if `hdl` is unknown.
  node_id lookup_direct(const connection_handle& hdl) const;

  /// Returns the handle offering a direct connection to `nid` or
  /// `invalid_connection_handle` if no direct connection to `nid` exists.
  connection_handle lookup_direct(const node_id& nid) const;

  /// Returns the next hop that would be chosen for `nid`
  /// or `invalid_node_id` if there's no indirect route to `nid`.
  node_id lookup_indirect(const node_id& nid) const;


  /// Flush output buffer for `r`.
  void flush(const route& r);

  /// Adds a new direct route to the table.
  /// @pre `hdl != invalid_connection_handle && nid != invalid_node_id`
  void add_direct(const connection_handle& hdl, const node_id& dest);

  /// Adds a new indirect route to the table.
  bool add_indirect(const node_id& hop, const node_id& dest);

  /// Blacklist the route to `dest` via `hop`.
  void blacklist(const node_id& hop, const node_id& dest);

  /// Removes a direct connection and calls `cb` for any node
  /// that became unreachable as a result of this operation,
  /// including the node that is assigned as direct path for `hdl`.
  void erase_direct(const connection_handle& hdl, erase_callback& cb);

  /// Removes any entry for indirect connection to `dest` and returns
  /// `true` if `dest` had an indirect route, otherwise `false`.
  bool erase_indirect(const node_id& dest);

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
    explicit callee(actor_namespace::backend& mgm, middleman& mm);

    virtual ~callee();

    /// Called if a server handshake was received and
    /// the connection to `nid` is established.
    virtual void finalize_handshake(const node_id& nid, actor_id aid,
                                    std::set<std::string>& sigs) = 0;

    /// Called whenever a direct connection was closed or a
    /// node became unrechable for other reasons *before*
    /// this node gets erased from the routing table.
    /// @warning The implementing class must not modify the
    ///          routing table from this callback.
    virtual void purge_state(const node_id& nid) = 0;

    /// Called whenever a remote node created a proxy
    /// for one of our local actors.
    virtual void proxy_announced(const node_id& nid, actor_id aid) = 0;

    /// Called whenever a remote actor died to destroy
    /// the proxy instance on our end.
    virtual void kill_proxy(const node_id& nid, actor_id aid, uint32_t rsn) = 0;

    /// Called whenever a `dispatch_message` arrived for a local actor.
    virtual void deliver(const node_id& source_node, actor_id source_actor,
                         const node_id& dest_node, actor_id dest_actor,
                         message& msg, message_id mid) = 0;

    /// Called whenever BASP learns the ID of a remote node
    /// to which it does not have a direct connection.
    virtual void learned_new_node_directly(const node_id& nid,
                                           bool was_known_indirectly) = 0;

    /// Called whenever BASP learns the ID of a remote node
    /// to which it does not have a direct connection.
    virtual void learned_new_node_indirectly(const node_id& nid) = 0;

    /// Returns the actor namespace associated to this BASP protocol instance.
    inline actor_namespace& get_namespace() {
      return namespace_;
    }

    inline middleman& get_middleman() {
      return middleman_;
    }

  protected:
    actor_namespace namespace_;
    middleman& middleman_;
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
  maybe<routing_table::route> lookup(const node_id& target);

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
             message_type operation,
             uint32_t* payload_len,
             uint64_t operation_data,
             const node_id& source_node,
             const node_id& dest_node,
             actor_id source_actor,
             actor_id dest_actor,
             payload_writer* writer = nullptr);

  /// Writes a header followed by its payload to `storage`.
  void write(buffer_type& storage, header& hdr,
             payload_writer* writer = nullptr);

  /// Writes the server handshake containing the information of the
  /// actor published at `port` to `buf`. If `port == none` or
  /// if no actor is published at this port then a standard handshake is
  /// written (e.g. used when establishing direct connections on-the-fly).
  void write_server_handshake(buffer_type& buf, maybe<uint16_t> port);

  /// Writes the client handshake to `buf`.
  void write_client_handshake(buffer_type& buf, const node_id& remote_side);

  /// Writes a `dispatch_error` to `buf`.
  void write_dispatch_error(buffer_type& buf,
                            const node_id& source_node,
                            const node_id& dest_node,
                            error error_code,
                            const header& original_hdr,
                            const buffer_type* payload);

  /// Writes a `kill_proxy_instance` to `buf`.
  void write_kill_proxy_instance(buffer_type& buf,
                                 const node_id& dest_node,
                                 actor_id aid,
                                 uint32_t rsn);

  inline const node_id& this_node() const {
    return this_node_;
  }

  /// Invokes the callback(s) associated with given event.
  template <hook::event_type Event, typename... Ts>
  void notify(Ts&&... xs) {
    callee_.get_middleman().template notify<Event>(std::forward<Ts>(xs)...);
  }

private:
  routing_table tbl_;
  published_actor_map published_actors_;
  node_id this_node_;
  callee& callee_;
};

/// Checks whether given header contains a handshake.
inline bool is_handshake(const header& hdr) {
  return hdr.operation == message_type::server_handshake
      || hdr.operation == message_type::client_handshake;
}

/// @}

} // namespace basp
} // namespace io
} // namespace caf

#endif // CAF_IO_BASP_HPP
