// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor_cast.hpp"
#include "caf/actor_clock.hpp"
#include "caf/actor_control_block.hpp"
#include "caf/fwd.hpp"
#include "caf/message.hpp"
#include "caf/message_priority.hpp"
#include "caf/send.hpp"

namespace caf::mixin {

/// A `sender` is an actor that supports `self->send(...)`.
template <class Base, class Subtype>
class sender : public Base {
public:
  // -- member types -----------------------------------------------------------

  using extended_base = sender;

  // -- constructors, destructors, and assignment operators --------------------

  using Base::Base;

  // -- send function family ---------------------------------------------------

  /// Sends `{args...}` as an asynchronous message to `receiver` with given
  /// priority.
  template <message_priority Priority = message_priority::normal, class Handle,
            class T, class... Ts>
  [[deprecated("use the mail API instead")]]
  void send(const Handle& receiver, T&& arg, Ts&&... args) {
    detail::send_type_check<typename Subtype::signatures, Handle, T, Ts...>();
    this->do_send(actor_cast<abstract_actor*>(receiver), Priority,
                  make_message_nowrap(std::forward<T>(arg),
                                      std::forward<Ts>(args)...));
  }

  /// Sends `{args...}` as an asynchronous message to `receiver` with given
  /// priority after the `timeout` expires.
  template <message_priority Priority = message_priority::normal, class Handle,
            class T, class... Ts>
  [[deprecated("use the mail API instead")]]
  disposable scheduled_send(const Handle& receiver,
                            actor_clock::time_point timeout, T&& arg,
                            Ts&&... args) {
    detail::send_type_check<typename Subtype::signatures, Handle, T, Ts...>();
    return this->do_scheduled_send(
      actor_cast<strong_actor_ptr>(receiver), Priority, timeout,
      make_message_nowrap(std::forward<T>(arg), std::forward<Ts>(args)...));
  }

  /// Sends `{args...}` as an asynchronous message to `receiver` with given
  /// priority after the `timeout` expires.
  template <message_priority Priority = message_priority::normal, class Handle,
            class T, class... Ts>
  [[deprecated("use the mail API instead")]]
  disposable delayed_send(const Handle& receiver,
                          actor_clock::duration_type timeout, T&& arg,
                          Ts&&... args) {
    return scheduled_send<Priority>(receiver, this->clock().now() + timeout,
                                    std::forward<T>(arg),
                                    std::forward<Ts>(args)...);
  }
};

} // namespace caf::mixin
