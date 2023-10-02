// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor.hpp"
#include "caf/actor_addr.hpp"
#include "caf/actor_cast.hpp"
#include "caf/check_typed_input.hpp"
#include "caf/local_actor.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/message.hpp"
#include "caf/message_id.hpp"
#include "caf/message_priority.hpp"
#include "caf/no_stages.hpp"
#include "caf/response_type.hpp"
#include "caf/system_messages.hpp"
#include "caf/typed_actor.hpp"

namespace caf {

/// Sends `to` a message under the identity of `from` with priority `prio`.
template <message_priority P = message_priority::normal, class Source = actor,
          class Dest = actor, class... Ts>
void send_as(const Source& src, const Dest& dest, Ts&&... xs) {
  static_assert(sizeof...(Ts) > 0, "no message to send");
  using token = detail::type_list<detail::strip_and_convert_t<Ts>...>;
  static_assert(!statically_typed<Source>() || statically_typed<Dest>(),
                "statically typed actors can only send() to other "
                "statically typed actors; use anon_send() or request() when "
                "communicating with dynamically typed actors");
  static_assert(response_type_unbox<signatures_of_t<Dest>, token>::valid,
                "receiver does not accept given message");
  // TODO: this only checks one way, we should check for loops
  static_assert(
    is_void_response<response_type_unbox_t<signatures_of_t<Dest>, token>>::value
      || response_type_unbox<
        signatures_of_t<Source>,
        response_type_unbox_t<signatures_of_t<Dest>, token>>::valid,
    "this actor does not accept the response message");
  if (dest)
    dest->eq_impl(make_message_id(P), actor_cast<strong_actor_ptr>(src),
                  nullptr, std::forward<Ts>(xs)...);
}

template <message_priority P = message_priority::normal, class Source,
          class Dest, class... Ts>
void unsafe_send_as(Source* src, const Dest& dest, Ts&&... xs) {
  static_assert(sizeof...(Ts) > 0, "no message to send");
  if (dest)
    actor_cast<abstract_actor*>(dest)->eq_impl(make_message_id(P), src->ctrl(),
                                               src->context(),
                                               std::forward<Ts>(xs)...);
}

template <class... Ts>
void unsafe_response(local_actor* self, strong_actor_ptr src,
                     std::vector<strong_actor_ptr> stages, message_id mid,
                     Ts&&... xs) {
  static_assert(sizeof...(Ts) > 0, "no message to send");
  strong_actor_ptr next;
  if (stages.empty()) {
    next = src;
    src = self->ctrl();
    if (mid.is_request())
      mid = mid.response_id();
  } else {
    next = std::move(stages.back());
    stages.pop_back();
  }
  if (next)
    next->enqueue(make_mailbox_element(std::move(src), mid, std::move(stages),
                                       std::forward<Ts>(xs)...),
                  self->context());
}

/// Anonymously sends `dest` a message.
template <message_priority P = message_priority::normal, class Dest = actor,
          class... Ts>
void anon_send(const Dest& dest, Ts&&... xs) {
  static_assert(sizeof...(Ts) > 0, "no message to send");
  using token = detail::type_list<detail::strip_and_convert_t<Ts>...>;
  static_assert(response_type_unbox<signatures_of_t<Dest>, token>::valid,
                "receiver does not accept given message");
  if (dest)
    dest->eq_impl(make_message_id(P), nullptr, nullptr,
                  std::forward<Ts>(xs)...);
}

template <message_priority P = message_priority::normal, class Dest = actor,
          class Rep = int, class Period = std::ratio<1>, class... Ts>
void delayed_anon_send(const Dest& dest,
                       std::chrono::duration<Rep, Period> rtime, Ts&&... xs) {
  static_assert(sizeof...(Ts) > 0, "no message to send");
  static_assert((detail::sendable<Ts> && ...),
                "at least one type has no ID, "
                "did you forgot to announce it via CAF_ADD_TYPE_ID?");
  using token = detail::type_list<detail::strip_and_convert_t<Ts>...>;
  static_assert(response_type_unbox<signatures_of_t<Dest>, token>::valid,
                "receiver does not accept given message");
  if (dest) {
    auto& clock = dest->home_system().clock();
    auto timeout = clock.now() + rtime;
    clock.schedule_message(timeout, actor_cast<strong_actor_ptr>(dest),
                           make_mailbox_element(nullptr, make_message_id(P),
                                                no_stages,
                                                std::forward<Ts>(xs)...));
  }
}

/// Anonymously sends `dest` an exit message.
template <class Dest>
void anon_send_exit(const Dest& dest, exit_reason reason) {
  CAF_LOG_TRACE(CAF_ARG(dest) << CAF_ARG(reason));
  if (dest)
    dest->enqueue(nullptr, make_message_id(),
                  make_message(exit_msg{dest->address(), reason}), nullptr);
}

/// Anonymously sends `to` an exit message.
inline void anon_send_exit(const actor_addr& to, exit_reason reason) {
  auto ptr = actor_cast<strong_actor_ptr>(to);
  if (ptr)
    anon_send_exit(ptr, reason);
}

/// Anonymously sends `to` an exit message.
inline void anon_send_exit(const weak_actor_ptr& to, exit_reason reason) {
  auto ptr = actor_cast<strong_actor_ptr>(to);
  if (ptr)
    anon_send_exit(ptr, reason);
}

} // namespace caf
