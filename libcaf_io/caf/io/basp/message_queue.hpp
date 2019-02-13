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

#include <cstdint>
#include <mutex>
#include <vector>

#include "caf/actor_control_block.hpp"
#include "caf/fwd.hpp"
#include "caf/mailbox_element.hpp"

namespace caf {
namespace io {
namespace basp {

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

  // -- mutators ---------------------------------------------------------------

  /// Adds a new message to the queue or deliver it immediately if possible.
  void push(execution_unit* ctx, uint64_t id, strong_actor_ptr receiver,
            mailbox_element_ptr content);

  /// Returns the next ascending ID.
  uint64_t new_id();
};

} // namespace basp
} // namespace io
} // namespace caf
