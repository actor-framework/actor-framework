// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/message_id.hpp"

#include <atomic>

namespace caf {

/// Interface for all message receivers. * This interface describes an
/// entity that can receive messages and is implemented by {@link actor}
/// and {@link group}.
class CAF_CORE_EXPORT abstract_channel {
public:
  friend class abstract_actor;
  friend class abstract_group;

  virtual ~abstract_channel();

  /// Enqueues a new message without forwarding stack to the channel.
  /// @returns `true` if the message has been dispatches successful, `false`
  ///          otherwise. In the latter case, the channel has been closed and
  ///          the message has been dropped. Once this function returns `false`,
  ///          it returns `false` for all future invocations.
  virtual bool enqueue(strong_actor_ptr sender, message_id mid, message content,
                       execution_unit* host = nullptr)
    = 0;

  static constexpr int is_abstract_actor_flag = 0x01000000;

  static constexpr int is_abstract_group_flag = 0x02000000;

  static constexpr int is_actor_bind_decorator_flag = 0x04000000;

  static constexpr int is_actor_dot_decorator_flag = 0x08000000;

  static constexpr int is_actor_decorator_mask = 0x0C000000;

  static constexpr int is_hidden_flag = 0x10000000;

  bool is_abstract_actor() const {
    return static_cast<bool>(flags() & is_abstract_actor_flag);
  }

  bool is_abstract_group() const {
    return static_cast<bool>(flags() & is_abstract_group_flag);
  }

  bool is_actor_decorator() const {
    return static_cast<bool>(flags() & is_actor_decorator_mask);
  }

protected:
  // note: *both* operations use relaxed memory order, this is because
  // only the actor itself is granted write access while all access
  // from other actors or threads is always read-only; further, only
  // flags that are considered constant after an actor has launched are
  // read by others, i.e., there is no acquire/release semantic between
  // setting and reading flags
  int flags() const {
    return flags_.load(std::memory_order_relaxed);
  }

  void flags(int new_value) {
    flags_.store(new_value, std::memory_order_relaxed);
  }

private:
  // can only be called from abstract_actor and abstract_group
  abstract_channel(int fs);

  // Accumulates several state and type flags. Subtypes may use only the
  // first 20 bits, i.e., the bitmask 0xFFF00000 is reserved for
  // channel-related flags.
  std::atomic<int> flags_;
};

} // namespace caf
