// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor_addr.hpp"
#include "caf/actor_cast.hpp"
#include "caf/fwd.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/message_id.hpp"
#include "caf/message_priority.hpp"
#include "caf/system_messages.hpp"

#include <type_traits>

namespace caf::detail {

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

} // namespace caf::detail

namespace caf {

/// Anonymously sends `dest` an exit message.
template <message_priority Priority = message_priority::normal, class Handle>
void anon_send_exit(const Handle& receiver, exit_reason reason) {
  if constexpr (std::is_same_v<Handle, actor_addr>) {
    anon_send_exit<Priority>(actor_cast<strong_actor_ptr>(receiver), reason);
  } else {
    if (receiver) {
      auto ptr = make_mailbox_element(nullptr, make_message_id(),
                                      exit_msg{receiver->address(), reason});
      receiver->enqueue(std::move(ptr), nullptr);
    }
  }
}

} // namespace caf
