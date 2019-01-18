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

#include <limits>

#include "caf/error.hpp"
#include "caf/variant.hpp"
#include "caf/defaults.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/binary_deserializer.hpp"

#include "caf/io/hook.hpp"
#include "caf/io/middleman.hpp"

#include "caf/io/basp/header.hpp"
#include "caf/io/basp/buffer_type.hpp"
#include "caf/io/basp/message_type.hpp"
#include "caf/io/basp/routing_table.hpp"
#include "caf/io/basp/connection_state.hpp"
#include "caf/io/basp/endpoint_context.hpp"

namespace caf {
namespace io {
namespace basp {

/// @addtogroup BASP

/// Describes a protocol instance managing multiple connections.
class instance {
public:
  using endpoint_handle = routing_table::endpoint_handle;
  /// Provides a callback-based interface for certain BASP events.
  class callee {
  protected:
    using buffer_type = std::vector<char>;
    using endpoint_handle = instance::endpoint_handle;
  public:
    explicit callee(actor_system& sys, proxy_registry::backend& backend);

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

    /// Called for each `dispatch_message` without `named_receiver_flag`.
    virtual void deliver(const node_id& source_node, actor_id source_actor,
                         actor_id dest_actor, message_id mid,
                         std::vector<strong_actor_ptr>& forwarding_stack,
                         message& msg) = 0;

    /// Called for each `dispatch_message` with `named_receiver_flag`.
    virtual void deliver(const node_id& source_node, actor_id source_actor,
                         atom_value dest_actor, message_id mid,
                         std::vector<strong_actor_ptr>& forwarding_stack,
                         message& msg) = 0;

    /// Called whenever BASP learns the ID of a remote node.
    virtual void learned_new_node(const node_id& nid) = 0;

    /// Get contact information for `nid` and establish communication.
    virtual void establish_communication(const node_id& nid) = 0;

    /// Called if a heartbeat was received from `nid`
    virtual void handle_heartbeat(const node_id& nid) = 0;

    /// Returns the actor namespace associated to this BASP protocol instance.
    inline proxy_registry& proxies() {
      return namespace_;
    }

    /// Returns the hosting actor system.
    inline actor_system& system() {
      return namespace_.system();
    }

    /// Returns the system-wide configuration.
    inline const actor_system_config& config() const {
      return namespace_.system().config();
    }

    /// Returns the next outgoing sequence number for a connection.
    virtual sequence_type next_sequence_number(connection_handle hdl) = 0;

    /// Returns the next outgoing sequence number for an endpoint.
    virtual sequence_type next_sequence_number(datagram_handle hdl) = 0;

    /// Adds a message with a future sequence number to the pending messages
    /// of a given endpoint context.
    virtual void add_pending(execution_unit* ctx, endpoint_context& ep,
                             sequence_type seq, header hdr,
                             std::vector<char> payload) = 0;

    /// Delivers a pending incoming messages for an endpoint `ep` with
    /// application layer ordering. Delivery of the next available packet can
    /// be forced to simply skip undeliverd messages via the `force` flag.
    virtual bool deliver_pending(execution_unit* ctx, endpoint_context& ep,
                                 bool force) = 0;

    /// Drop pending messages with sequence number `seq`.
    virtual void drop_pending(endpoint_context& ep, sequence_type seq) = 0;

    /// Send messages that were buffered while connectivity establishment
    /// was pending using `hdl`.
    virtual void send_buffered_messages(execution_unit* ctx, node_id nid,
                                        connection_handle hdl) = 0;

    /// Send messages that were buffered while connectivity establishment
    /// was pending using `hdl`.
    virtual void send_buffered_messages(execution_unit* ctx, node_id nid,
                                        datagram_handle hdl) = 0;

    /// Returns a reference to the current sent buffer, dispatching the call
    /// based on the type contained in `hdl`.
    virtual buffer_type& get_buffer(endpoint_handle hdl) = 0;

    /// Returns a reference to the current sent buffer. The callee may cache
    /// buffers to reuse them for multiple datagrams. Subsequent calls will
    /// return the same buffer until `pop_datagram_buffer` is called.
    virtual buffer_type& get_buffer(datagram_handle hdl) = 0;

    /// Returns a reference to the sent buffer.
    virtual buffer_type& get_buffer(connection_handle hdl) = 0;

    /// Returns a reference to a buffer to be sent to node with `nid`.
    /// If communication with the node is esstablished, it picks the first
    /// available handle, otherwise a buffer for a pending message is returned.
    virtual buffer_type& get_buffer(node_id nid) = 0;

    /// Returns the buffer accessed through a call to `get_buffer` when
    /// passing a datagram handle and removes it from the callee.
    virtual buffer_type pop_datagram_buffer(datagram_handle hdl) = 0;

    /// Flushes the underlying write buffer of `hdl`, dispatches the call based
    /// on the type contained in `hdl`.
    virtual void flush(endpoint_handle hdl) = 0;

    /// Flushes the underlying write buffer of `hdl`. Implicitly pops the
    /// current buffer and enqueues it for sending.
    virtual void flush(datagram_handle hdl) = 0;

    /// Flushes the underlying write buffer of `hdl`.
    virtual void flush(connection_handle hdl) = 0;

  protected:
    proxy_registry namespace_;
  };

  /// Describes a function object responsible for writing
  /// the payload for a BASP message.
  using payload_writer = callback<serializer&>;

  /// Describes a callback function object for `remove_published_actor`.
  using removed_published_actor = callback<const strong_actor_ptr&, uint16_t>;

  instance(abstract_broker* parent, callee& lstnr);

  /// Handles received data and returns a config for receiving the
  /// next data or `none` if an error occured.
  connection_state handle(execution_unit* ctx,
                          new_data_msg& dm, header& hdr, bool is_payload);

  /// Handles a received datagram.
  bool handle(execution_unit* ctx, new_datagram_msg& dm, endpoint_context& ep);

  /// Sends heartbeat messages to all valid nodes those are directly connected.
  void handle_heartbeat(execution_unit* ctx);

  /// Flushes the underlying buffer of `hdl`.
  void flush(endpoint_handle hdl);

  /// Sends a BASP message and implicitly flushes the output buffer of `hdl`.
  /// This function will update `hdr.payload_len` if a payload was written.
  void write(execution_unit* ctx, endpoint_handle hdl,
             header& hdr, payload_writer* writer = nullptr);

  /// Adds a new actor to the map of published actors.
  void add_published_actor(uint16_t port,
                           strong_actor_ptr published_actor,
                           std::set<std::string> published_interface);

  /// Removes the actor currently assigned to `port`.
  size_t remove_published_actor(uint16_t port,
                                removed_published_actor* cb = nullptr);

  /// Removes `whom` if it is still assigned to `port` or from all of its
  /// current ports if `port == 0`.
  size_t remove_published_actor(const actor_addr& whom, uint16_t port,
                                removed_published_actor* cb = nullptr);

  /// Compare two sequence numbers
  static bool is_greater(sequence_type lhs, sequence_type rhs,
                         sequence_type max_distance
                          = std::numeric_limits<sequence_type>::max() / 2);


  /// Returns `true` if a path to destination existed, `false` otherwise.
  bool dispatch(execution_unit* ctx, const strong_actor_ptr& sender,
                const std::vector<strong_actor_ptr>& forwarding_stack,
                const strong_actor_ptr& receiver,
                message_id mid, const message& msg);

  /// Returns the actor namespace associated to this BASP protocol instance.
  proxy_registry& proxies() {
    return callee_.proxies();
  }

  /// Returns the routing table of this BASP instance.
  routing_table& tbl() {
    return tbl_;
  }

  /// Stores the address of a published actor along with its publicly
  /// visible messaging interface.
  using published_actor = std::pair<strong_actor_ptr, std::set<std::string>>;

  /// Maps ports to addresses and interfaces of published actors.
  using published_actor_map = std::unordered_map<uint16_t, published_actor>;

  /// Returns the current mapping of ports to addresses
  /// and interfaces of published actors.
  inline const published_actor_map& published_actors() const {
    return published_actors_;
  }

  /// Writes a header followed by its payload to `storage`.
  static void write(execution_unit* ctx, buffer_type& buf, header& hdr,
                    payload_writer* pw = nullptr);

  /// Writes the server handshake containing the information of the
  /// actor published at `port` to `buf`. If `port == none` or
  /// if no actor is published at this port then a standard handshake is
  /// written (e.g. used when establishing direct connections on-the-fly).
  void write_server_handshake(execution_unit* ctx,
                              buffer_type& out_buf, optional<uint16_t> port,
                              uint16_t sequence_number = 0);

  /// Writes the client handshake to `buf`.
  void write_client_handshake(execution_unit* ctx,
                              buffer_type& buf,
                              const node_id& remote_side,
                              const node_id& this_node,
                              const std::string& app_identifier,
                              uint16_t sequence_number = 0);

  /// Writes the client handshake to `buf`.
  void write_client_handshake(execution_unit* ctx,
                              buffer_type& buf, const node_id& remote_side,
                              uint16_t sequence_number = 0);

  /// Writes the acknowledge handshake to finish a UDP handshake to `buf`.
  void write_acknowledge_handshake(execution_unit* ctx,
                                   buffer_type& buf, const node_id& remote_side,
                                   uint16_t sequence_number = 0);

  /// Writes an `announce_proxy` to `buf`.
  void write_announce_proxy(execution_unit* ctx, buffer_type& buf,
                            const node_id& dest_node, actor_id aid,
                            uint16_t sequence_number = 0);

  /// Writes a `kill_proxy` to `buf`.
  void write_kill_proxy(execution_unit* ctx, buffer_type& buf,
                        const node_id& dest_node, actor_id aid,
                        const error& rsn, uint16_t sequence_number = 0);

  /// Writes a `heartbeat` to `buf`.
  void write_heartbeat(execution_unit* ctx,
                       buffer_type& buf, const node_id& remote_side,
                       uint16_t sequence_number = 0);

  inline const node_id& this_node() const {
    return this_node_;
  }

  /// Invokes the callback(s) associated with given event.
  template <hook::event_type Event, typename... Ts>
  void notify(Ts&&... xs) {
    system().middleman().template notify<Event>(std::forward<Ts>(xs)...);
  }

  inline actor_system& system() {
    return callee_.system();
  }

  template <class Handle>
  bool handle(execution_unit* ctx, const Handle& hdl, header& hdr,
              std::vector<char>* payload, bool tcp_based,
              optional<endpoint_context&> ep, optional<uint16_t> port) {
    // Function object for checking payload validity.
    auto payload_valid = [&]() -> bool {
      return payload != nullptr && payload->size() == hdr.payload_len;
    };
    // Handle message to ourselves.
    switch (hdr.operation) {
      case message_type::server_handshake: {
        if (!payload_valid())
          return false;
        binary_deserializer bd{ctx, *payload};
        std::string remote_appid;
        actor_id aid = invalid_actor_id;
        std::set<std::string> sigs;
        basp::routing_table::address_map addrs;
        if (auto err = bd(remote_appid, aid, sigs, addrs)) {
          CAF_LOG_ERROR("unable to deserialize payload:"
                        << callee_.system().render(err));
          return false;
        }
        if (remote_appid != get_or(callee_.config(), "middleman.app-identifier",
                                   defaults::middleman::app_identifier)) {
          CAF_LOG_ERROR("app identifier mismatch");
          return false;
        }
        // Close self connection after handshake is done.
        if (hdr.source_node == this_node_) {
          CAF_LOG_DEBUG("close connection to self immediately");
          callee_.finalize_handshake(hdr.source_node, aid, sigs);
          return false;
        }
        // Close this connection if we already established communication.
        auto lr = tbl_.lookup(hdr.source_node);
        // TODO: Anything additional or different for UDP?
        if (lr.hdl) {
          CAF_LOG_INFO("close redundant" << CAF_ARG(hdr.source_node));
          callee_.finalize_handshake(hdr.source_node, aid, sigs);
          return false;
        } else if (!tcp_based && (tbl_.received_client_handshake(hdr.source_node)
                   && this_node() < hdr.source_node)) {
          CAF_LOG_INFO("simultaneous handshake, let the other node proceed: "
                       << CAF_ARG(hdr.source_node));
          callee_.finalize_handshake(hdr.source_node, aid, sigs);
          return false;
        }
        // Add this node to our contacts.
        CAF_LOG_INFO("new endpoint:" << CAF_ARG(hdr.source_node));
        if (lr.known)
          tbl_.handle(hdr.source_node, hdl);
        else
          tbl_.add(hdr.source_node, hdl);
        auto peer_server = system().registry().get(atom("PeerServ"));
        anon_send(actor_cast<actor>(peer_server), put_atom::value,
                  to_string(hdr.source_node), make_message(addrs));
        // Write handshake as client in response.
        if (tcp_based) {
          write_client_handshake(ctx, callee_.get_buffer(hdl), hdr.source_node);
        } else {
          auto seq = ep->requires_ordering ? ep->seq_outgoing++ : 0;
          write_acknowledge_handshake(ctx, callee_.get_buffer(hdl),
                                      hdr.source_node, seq);
        }
        flush(hdl);
        callee_.learned_new_node(hdr.source_node);
        callee_.finalize_handshake(hdr.source_node, aid, sigs);
        callee_.send_buffered_messages(ctx, hdr.source_node, hdl);
        break;
      }
      case message_type::client_handshake: {
        if (!payload_valid()) {
          CAF_LOG_ERROR("fail to receive the app identifier");
          return false;
        }
        binary_deserializer bd{ctx, *payload};
        std::string remote_appid;
        basp::routing_table::address_map addrs;
        if (auto err = bd(remote_appid, addrs)) {
          CAF_LOG_ERROR("unable to deserialize payload:"
                        << callee_.system().render(err));
          return false;
        }
        if (remote_appid != get_or(callee_.config(), "middleman.app-identifier",
                                   defaults::middleman::app_identifier)) {
          CAF_LOG_ERROR("app identifier mismatch");
          return false;
        }
        // Handshakes were only exchanged if `hdl` is set.
        auto lr = tbl_.lookup(hdr.source_node);
        auto new_node = (this_node() != hdr.source_node && !lr.hdl);
        if (tcp_based) {
          if (!new_node) {
            CAF_LOG_DEBUG("received second client handshake:"
                          << CAF_ARG(hdr.source_node));
            break;
          }
          // Add this node to our contacts.
          CAF_LOG_DEBUG("new endpoint:" << CAF_ARG(hdr.source_node));
          // Either add a new node or add the handle to a known one.
          if (lr.known)
            tbl_.handle(hdr.source_node, hdl);
          else
            tbl_.add(hdr.source_node, hdl);
          callee_.learned_new_node(hdr.source_node);
          callee_.send_buffered_messages(ctx, hdr.source_node, hdl);
        } else {
          if (!lr.known)
            tbl_.add(hdr.source_node);
          tbl_.received_client_handshake(hdr.source_node, true);
          uint16_t seq = ep->requires_ordering ? ep->seq_outgoing++ : 0;
          write_server_handshake(ctx, callee_.get_buffer(hdl), port, seq);
          callee_.flush(hdl);
        }
        auto peer_server = system().registry().get(atom("PeerServ"));
        anon_send(actor_cast<actor>(peer_server), put_atom::value,
                  to_string(hdr.source_node), make_message(addrs));
        break;
      }
      case message_type::acknowledge_handshake: {
        if (tcp_based) {
          CAF_LOG_ERROR("received acknowledge handshake via tcp");
          break;
        }
        auto lr = tbl_.lookup(hdr.source_node);
        if (!lr.known) {
          CAF_LOG_DEBUG("dropping acknowledge handshake from unknown node");
          break;
        }
        if (lr.hdl) {
          CAF_LOG_DEBUG("dropping repeated acknowledge handshake");
          // TODO: Or should we just adopt the new handle?
          break;
        }
        CAF_LOG_INFO("new endpoint:" << CAF_ARG(hdr.source_node));
        tbl_.handle(hdr.source_node, hdl);
        tbl_.received_client_handshake(hdr.source_node, false);
        callee_.learned_new_node(hdr.source_node);
        callee_.send_buffered_messages(ctx, hdr.source_node, hdl);
        break;
      }
      case message_type::dispatch_message: {
        if (!payload_valid())
          return false;
        binary_deserializer bd{ctx, *payload};
        auto receiver_name = static_cast<atom_value>(0);
        std::vector<strong_actor_ptr> forwarding_stack;
        message msg;
        if (hdr.has(header::named_receiver_flag)) {
          auto e = bd(receiver_name);
          if (e)
            return false;
        }
        auto e = bd(forwarding_stack, msg);
        if (e)
          return false;
        CAF_LOG_DEBUG(CAF_ARG(forwarding_stack) << CAF_ARG(msg));
        if (hdr.has(header::named_receiver_flag))
          callee_.deliver(hdr.source_node, hdr.source_actor, receiver_name,
                          make_message_id(hdr.operation_data),
                          forwarding_stack, msg);
        else
          callee_.deliver(hdr.source_node, hdr.source_actor, hdr.dest_actor,
                          make_message_id(hdr.operation_data),
                          forwarding_stack, msg);
        break;
      }
      case message_type::announce_proxy:
        callee_.proxy_announced(hdr.source_node, hdr.dest_actor);
        break;
      case message_type::kill_proxy: {
        if (!payload_valid())
          return false;
        binary_deserializer bd{ctx, *payload};
        error fail_state;
        auto e = bd(fail_state);
        if (e)
          return false;
        callee_.proxies().erase(hdr.source_node, hdr.source_actor,
                                std::move(fail_state));
        break;
      }
      case message_type::heartbeat: {
        CAF_LOG_TRACE("received heartbeat: " << CAF_ARG(hdr.source_node));
        callee_.handle_heartbeat(hdr.source_node);
        break;
      }
      default:
        CAF_LOG_ERROR("invalid operation");
        return false;
    }
    return true;
  }

private:
  routing_table tbl_;
  published_actor_map published_actors_;
  node_id this_node_;
  callee& callee_;
};

/// @}

} // namespace basp
} // namespace io
} // namespace caf

