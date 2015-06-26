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

#ifndef CAF_ABSTRACT_CHANNEL_HPP
#define CAF_ABSTRACT_CHANNEL_HPP

#include <atomic>

#include "caf/fwd.hpp"
#include "caf/node_id.hpp"
#include "caf/message_id.hpp"
#include "caf/ref_counted.hpp"

namespace caf {

/// Interface for all message receivers. * This interface describes an
/// entity that can receive messages and is implemented by {@link actor}
/// and {@link group}.
class abstract_channel : public ref_counted {
public:
  friend class abstract_actor;
  friend class abstract_group;

  virtual ~abstract_channel();

  /// Enqueues a new message to the channel.
  virtual void enqueue(const actor_addr& sender, message_id mid,
                       message content, execution_unit* host) = 0;

  /// Enqueues a new message wrapped in a `mailbox_element` to the channel.
  /// This variant is used by actors whenever it is possible to allocate
  /// mailbox element and message on the same memory block and is thus
  /// more efficient. Non-actors use the default implementation which simply
  /// calls the pure virtual version.
  virtual void enqueue(mailbox_element_ptr what, execution_unit* host);

  /// Returns the ID of the node this actor is running on.
  inline node_id node() const {
    return node_;
  }

  /// Returns true if {@link node_ptr} returns
  bool is_remote() const;

  static constexpr int is_abstract_actor_flag = 0x100000;

  static constexpr int is_abstract_group_flag = 0x200000;

  inline bool is_abstract_actor() const {
    return static_cast<bool>(flags() & is_abstract_actor_flag);
  }

  inline bool is_abstract_group() const {
    return static_cast<bool>(flags() & is_abstract_group_flag);
  }

protected:
  // note: *both* operations use relaxed memory order, this is because
  // only the actor itself is granted write access while all access
  // from other actors or threads is always read-only; further, only
  // flags that are considered constant after an actor has launched are
  // read by others, i.e., there is no acquire/release semantic between
  // setting and reading flags
  inline int flags() const {
    return flags_.load(std::memory_order_relaxed);
  }

  inline void flags(int new_value) {
    flags_.store(new_value, std::memory_order_relaxed);
  }

private:
  // can only be called from abstract_actor and abstract_group
  explicit abstract_channel(int init_flags);
  abstract_channel(int init_flags, node_id nid);

  /*
   * Accumulates several state and type flags. Subtypes may use only the
   * first 20 bits, i.e., the bitmask 0xFFF00000 is reserved for
   * channel-related flags.
   */
  std::atomic<int> flags_;
  // identifies the node of this channel
  node_id node_;
};

/// A smart pointer to an abstract channel.
/// @relates abstract_channel_ptr
using abstract_channel_ptr = intrusive_ptr<abstract_channel>;

} // namespace caf

#endif // CAF_ABSTRACT_CHANNEL_HPP
