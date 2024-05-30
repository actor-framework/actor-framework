// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor_addr.hpp"
#include "caf/actor_cast.hpp"
#include "caf/actor_clock.hpp"
#include "caf/detail/send_type_check.hpp"
#include "caf/disposable.hpp"
#include "caf/fwd.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/message_id.hpp"
#include "caf/message_priority.hpp"
#include "caf/system_messages.hpp"
#include "caf/typed_actor.hpp"

namespace caf::detail {

/// Sends a message to `receiver` under the identity of `sender`.
template <message_priority Priority = message_priority::normal, class Source,
          class Dest, class T, class... Ts>
void send_as(const Source& sender, const Dest& receiver, T&& arg,
             Ts&&... args) {
  send_type_check<signatures_of_t<Source>, Dest, T, Ts...>();
  if (receiver)
    receiver->enqueue(make_mailbox_element(actor_cast<strong_actor_ptr>(sender),
                                           make_message_id(Priority),
                                           std::forward<T>(arg),
                                           std::forward<Ts>(args)...),
                      nullptr);
}

/// Sends a message to `receiver` under the identity of `sender` without type
/// checking.
template <message_priority Priority = message_priority::normal, class Source,
          class Dest, class T, class... Ts>
void unsafe_send_as(Source* src, const Dest& receiver, T&& arg, Ts&&... args) {
  if (receiver)
    receiver->enqueue(
      make_mailbox_element(src->ctrl(), make_message_id(Priority),
                           std::forward<T>(arg), std::forward<Ts>(args)...),
      nullptr);
}

/// Sends an asynchronous, anonymous message to `receiver`without type checking.
template <message_priority Priority = message_priority::normal, class Handle,
          class T, class... Ts>
void unsafe_anon_send(const Handle& receiver, T&& arg, Ts&&... args) {
  if (receiver)
    receiver->enqueue(make_mailbox_element(nullptr, make_message_id(Priority),
                                           std::forward<T>(arg),
                                           std::forward<Ts>(args)...),
                      nullptr);
}

} // namespace caf::detail

namespace caf {

/// Sends an asynchronous, anonymous message to `receiver`.
template <message_priority Priority = message_priority::normal, class Handle,
          class T, class... Ts>
[[deprecated("use anon_mail instead")]]
void anon_send(const Handle& receiver, T&& arg, Ts&&... args) {
  detail::send_type_check<none_t, Handle, T, Ts...>();
  if (receiver)
    receiver->enqueue(make_mailbox_element(nullptr, make_message_id(Priority),
                                           std::forward<T>(arg),
                                           std::forward<Ts>(args)...),
                      nullptr);
}

/// Sends an asynchronous, anonymous message to `receiver` after the timeout.
template <message_priority Priority = message_priority::normal, class Handle,
          class T, class... Ts>
[[deprecated("use anon_mail instead")]]
disposable delayed_anon_send(const Handle& receiver,
                             actor_clock::duration_type timeout, T&& arg,
                             Ts&&... args) {
  detail::send_type_check<none_t, Handle, T, Ts...>();
  if (!receiver)
    return {};
  auto& clock
    = actor_cast<actor_control_block*>(receiver)->home_system->clock();
  return clock.schedule_message(
    clock.now() + timeout, actor_cast<strong_actor_ptr>(receiver),
    make_mailbox_element(nullptr, make_message_id(Priority),
                         std::forward<T>(arg), std::forward<T>(args)...));
}

/// Sends an asynchronous, anonymous message to `receiver` after the timeout.
template <message_priority Priority = message_priority::normal, class Handle,
          class T, class... Ts>
[[deprecated("use anon_mail instead")]]
disposable scheduled_anon_send(const Handle& receiver,
                               actor_clock::time_point timeout, T&& arg,
                               Ts&&... args) {
  detail::send_type_check<none_t, Handle, T, Ts...>();
  if (!receiver)
    return {};
  auto& clock
    = actor_cast<actor_control_block*>(receiver)->home_system->clock();
  return clock.schedule_message(
    timeout, actor_cast<strong_actor_ptr>(receiver),
    make_mailbox_element(nullptr, make_message_id(Priority),
                         std::forward<T>(arg), std::forward<T>(args)...));
}

/// Anonymously sends `dest` an exit message.
template <message_priority Priority = message_priority::normal, class Handle>
void anon_send_exit(const Handle& receiver, exit_reason reason) {
  if (receiver) {
    auto ptr = make_mailbox_element(nullptr, make_message_id(),
                                    exit_msg{receiver->address(), reason});
    receiver->enqueue(std::move(ptr), nullptr);
  }
}

/// Anonymously sends `to` an exit message.
template <message_priority Priority = message_priority::normal>
void anon_send_exit(const actor_addr& receiver, exit_reason reason) {
  if (auto ptr = actor_cast<strong_actor_ptr>(receiver)) {
    auto item = make_mailbox_element(nullptr, make_message_id(Priority),
                                     exit_msg{receiver, reason});
    ptr->enqueue(std::move(item), nullptr);
  }
}

} // namespace caf
