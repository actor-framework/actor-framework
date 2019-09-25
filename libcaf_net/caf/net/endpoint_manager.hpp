/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
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

#include <cstddef>
#include <cstdint>
#include <memory>

#include "caf/actor.hpp"
#include "caf/actor_clock.hpp"
#include "caf/byte.hpp"
#include "caf/fwd.hpp"
#include "caf/intrusive/drr_queue.hpp"
#include "caf/intrusive/fifo_inbox.hpp"
#include "caf/intrusive/singly_linked.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/variant.hpp"

namespace caf {
namespace net {

/// Manages a communication endpoint.
class endpoint_manager : public socket_manager {
public:
  // -- member types -----------------------------------------------------------

  using super = socket_manager;

  /// Represents either an error or a serialized payload.
  using maybe_buffer = expected<std::vector<byte>>;

  /// A function type for serializing message payloads.
  using serialize_fun_type = maybe_buffer (*)(actor_system&,
                                              const type_erased_tuple&);

  struct event : intrusive::singly_linked<event> {
    struct resolve_request {
      std::string path;
      actor listener;
    };

    struct timeout {
      atom_value type;
      uint64_t id;
    };

    event(std::string path, actor listener);

    event(atom_value type, uint64_t id);

    /// Either contains a string for `resolve` requests or an `atom_value`
    variant<resolve_request, timeout> value;
  };

  struct event_policy {
    using deficit_type = size_t;

    using task_size_type = size_t;

    using mapped_type = event;

    using unique_pointer = std::unique_ptr<event>;

    using queue_type = intrusive::drr_queue<event_policy>;

    task_size_type task_size(const event&) const noexcept {
      return 1;
    }
  };

  using event_queue_type = intrusive::fifo_inbox<event_policy>;

  struct message : intrusive::singly_linked<message> {
    /// Original message to a remote actor.
    mailbox_element_ptr msg;

    /// ID of the receiving actor.
    strong_actor_ptr receiver;

    /// Serialized representation of of `msg->content()`.
    std::vector<byte> payload;

    message(mailbox_element_ptr msg, strong_actor_ptr receiver,
            std::vector<byte> payload);
  };

  struct message_policy {
    using deficit_type = size_t;

    using task_size_type = size_t;

    using mapped_type = message;

    using unique_pointer = std::unique_ptr<message>;

    using queue_type = intrusive::drr_queue<message_policy>;

    task_size_type task_size(const message& x) const noexcept {
      // Return at least 1 if the payload is empty.
      return x.payload.size() + static_cast<task_size_type>(x.payload.empty());
    }
  };

  using message_queue_type = intrusive::fifo_inbox<message_policy>;

  // -- constructors, destructors, and assignment operators --------------------

  endpoint_manager(socket handle, const multiplexer_ptr& parent,
                   actor_system& sys);

  ~endpoint_manager() override;

  // -- properties -------------------------------------------------------------

  actor_system& system() {
    return sys_;
  }

  std::unique_ptr<message> next_message();

  // -- event management -------------------------------------------------------

  /// Resolves a path to a remote actor.
  void resolve(std::string path, actor listener);

  /// Enqueues a message to the endpoint.
  void enqueue(mailbox_element_ptr msg, strong_actor_ptr receiver,
               std::vector<byte> payload);

  /// Enqueues a timeout to the endpoint.
  void enqueue(timeout_msg msg);

  // -- pure virtual member functions ------------------------------------------

  /// Initializes the manager before adding it to the multiplexer's event loop.
  virtual error init() = 0;

  /// @returns the protocol-specific function for serializing payloads.
  virtual serialize_fun_type serialize_fun() const noexcept = 0;

protected:
  /// Points to the hosting actor system.
  actor_system& sys_;

  /// Stores control events.
  event_queue_type events_;

  /// Stores outbound messages.
  message_queue_type messages_;

  /// Stores a proxy for interacting with the actor clock.
  actor timeout_proxy_;
};

using endpoint_manager_ptr = intrusive_ptr<endpoint_manager>;

} // namespace net
} // namespace caf
