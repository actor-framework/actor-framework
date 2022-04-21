// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

#include "caf/actor_control_block.hpp"
#include "caf/fwd.hpp"
#include "caf/mailbox_element.hpp"

namespace caf::net::basp {

/// Enforces strict order of message delivery, i.e., deliver messages in the
/// same order as if they were deserialized by a single thread.
class message_queue {
public:
  // -- member types -----------------------------------------------------------

  /// Request for sending a message to an actor at a later time.
  struct actor_msg {
    uint64_t id;
    strong_actor_ptr receiver;
    mailbox_element_ptr content;
  };

  // -- constructors, destructors, and assignment operators --------------------

  message_queue();

  // -- mutators ---------------------------------------------------------------

  /// Adds a new message to the queue or deliver it immediately if possible.
  void push(execution_unit* ctx, uint64_t id, strong_actor_ptr receiver,
            mailbox_element_ptr content);

  /// Marks given ID as dropped, effectively skipping it without effect.
  void drop(execution_unit* ctx, uint64_t id);

  /// Returns the next ascending ID.
  uint64_t new_id();

  // -- member variables -------------------------------------------------------

  /// Protects all other properties.
  std::mutex lock;

  /// The next available ascending ID. The counter is large enough to overflow
  /// after roughly 600 years if we dispatch a message every microsecond.
  uint64_t next_id;

  /// The next ID that we can ship.
  uint64_t next_undelivered;

  /// Keeps messages in sorted order in case a message other than
  /// `next_undelivered` gets ready first.
  std::vector<actor_msg> pending;
};

} // namespace caf::net::basp
