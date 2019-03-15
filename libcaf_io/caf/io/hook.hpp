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

#include <set>
#include <string>
#include <memory>
#include <vector>

#include "caf/fwd.hpp"

namespace caf {
namespace io {

class hook;

// safes us some typing for the static dispatching
#define CAF_IO_HOOK_DISPATCH(eventname)                                        \
  template <typename... Ts>                                                    \
  void dispatch(event<eventname>, Ts&&... ts) {                                \
    eventname##_cb(std::forward<Ts>(ts)...);                                   \
  }

/// @relates hook
using hook_uptr = std::unique_ptr<hook>;

/// Interface to define hooks into the IO layer.
class hook {
public:
  explicit hook(actor_system& sys);

  virtual ~hook();

  /// Called whenever a message has arrived via the network.
  virtual void message_received_cb(const node_id& source,
                                   const strong_actor_ptr& from,
                                   const strong_actor_ptr& dest,
                                   message_id mid,
                                   const message& msg);

  /// Called whenever a message has been sent to the network.
  /// @param from The address of the sending actor.
  /// @param hop The node in the network we've sent the message to.
  /// @param dest The address of the receiving actor. Note that the node ID
  ///             of `dest` can differ from `hop` in case we don't
  ///             have a direct connection to `dest_node`.
  /// @param mid The ID of the message.
  /// @param payload The message we've sent.
  virtual void message_sent_cb(const strong_actor_ptr& from, const node_id& hop,
                               const strong_actor_ptr& dest, message_id mid,
                               const message& payload);

  /// Called whenever no route for sending a message exists.
  virtual void message_sending_failed_cb(const strong_actor_ptr& from,
                                         const strong_actor_ptr& dest,
                                         message_id mid,
                                         const message& payload);

  /// Called whenever a message is forwarded to a different node.
  virtual void message_forwarded_cb(const basp::header& hdr,
                                    const std::vector<char>* payload);

  /// Called whenever no route for a forwarding request exists.
  virtual void message_forwarding_failed_cb(const basp::header& hdr,
                                            const std::vector<char>* payload);

  /// Called whenever an actor has been published.
  virtual void actor_published_cb(const strong_actor_ptr& addr,
                                  const std::set<std::string>& ifs,
                                  uint16_t port);

  /// Called whenever a new remote actor appeared.
  virtual void new_remote_actor_cb(const strong_actor_ptr& addr);

  /// Called whenever a handshake via a direct TCP connection succeeded.
  virtual void new_connection_established_cb(const node_id& node);

  /// Called whenever a message from or to a yet unknown node was received.
  /// @param via The node that has sent us the message.
  /// @param node The newly added entry to the routing table.
  virtual void new_route_added_cb(const node_id& via, const node_id& node);

  /// Called whenever a direct connection was lost.
  virtual void connection_lost_cb(const node_id& dest);

  /// Called whenever a route became unavailable.
  /// @param hop The node that was either disconnected
  ///            or lost a connection itself.
  /// @param dest The node that is no longer reachable via `hop`.
  virtual void route_lost_cb(const node_id& hop, const node_id& dest);

  /// Called whenever a message was discarded because a remote node
  /// tried to send a message to an actor ID that could not be found
  /// in the registry.
  virtual void invalid_message_received_cb(const node_id& source,
                                           const strong_actor_ptr& sender,
                                           actor_id invalid_dest,
                                           message_id mid, const message& msg);

  /// Called before middleman shuts down.
  virtual void before_shutdown_cb();

  /// All possible events for IO hooks.
  enum event_type {
    message_received,
    message_sent,
    message_forwarded,
    message_sending_failed,
    message_forwarding_failed,
    actor_published,
    new_remote_actor,
    new_connection_established,
    new_route_added,
    connection_lost,
    route_lost,
    invalid_message_received,
    before_shutdown
  };

  /// Handles an event by invoking the associated callback.
  template <event_type Event, typename... Ts>
  void handle(Ts&&... ts) {
    dispatch(event<Event>{}, std::forward<Ts>(ts)...);
  }

  inline actor_system& system() const {
    return system_;
  }

private:
  // ------------ convenience interface based on static dispatching ------------
  template <event_type Id>
  using event = std::integral_constant<event_type, Id>;

  CAF_IO_HOOK_DISPATCH(message_received)
  CAF_IO_HOOK_DISPATCH(message_sent)
  CAF_IO_HOOK_DISPATCH(message_forwarded)
  CAF_IO_HOOK_DISPATCH(message_sending_failed)
  CAF_IO_HOOK_DISPATCH(message_forwarding_failed)
  CAF_IO_HOOK_DISPATCH(actor_published)
  CAF_IO_HOOK_DISPATCH(new_remote_actor)
  CAF_IO_HOOK_DISPATCH(new_connection_established)
  CAF_IO_HOOK_DISPATCH(new_route_added)
  CAF_IO_HOOK_DISPATCH(connection_lost)
  CAF_IO_HOOK_DISPATCH(route_lost)
  CAF_IO_HOOK_DISPATCH(invalid_message_received)
  CAF_IO_HOOK_DISPATCH(before_shutdown)

  actor_system& system_;
};

} // namespace io
} // namespace caf
