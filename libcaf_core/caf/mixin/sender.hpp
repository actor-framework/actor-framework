// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor.hpp"
#include "caf/actor_cast.hpp"
#include "caf/actor_clock.hpp"
#include "caf/actor_control_block.hpp"
#include "caf/check_typed_input.hpp"
#include "caf/detail/profiled_send.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/fwd.hpp"
#include "caf/message.hpp"
#include "caf/message_priority.hpp"
#include "caf/no_stages.hpp"
#include "caf/response_handle.hpp"
#include "caf/response_type.hpp"
#include "caf/scheduler/abstract_coordinator.hpp"
#include "caf/send.hpp"
#include "caf/typed_actor_view_base.hpp"

#include <chrono>
#include <tuple>

namespace caf::mixin {

/// A `sender` is an actor that supports `self->send(...)`.
template <class Base, class Subtype>
class sender : public Base {
public:
  // -- member types -----------------------------------------------------------

  using extended_base = sender;

  // -- constructors, destructors, and assignment operators --------------------

  template <class... Ts>
  sender(Ts&&... xs) : Base(std::forward<Ts>(xs)...) {
    // nop
  }

  // -- send function family ---------------------------------------------------

  /// Sends `{xs...}` as an asynchronous message to `dest` with priority `mp`.
  template <message_priority P = message_priority::normal, class Dest,
            class... Ts>
  void send(const Dest& dest, Ts&&... xs) {
    static_assert(sizeof...(Ts) > 0, "no message to send");
    static_assert((detail::sendable<Ts> && ...),
                  "at least one type has no ID, "
                  "did you forgot to announce it via CAF_ADD_TYPE_ID?");
    detail::type_list<detail::strip_and_convert_t<Ts>...> args_token;
    type_check(dest, args_token);
    auto self = dptr();
    detail::profiled_send(self, self->ctrl(), dest, make_message_id(P), {},
                          self->context(), std::forward<Ts>(xs)...);
  }

  template <message_priority P = message_priority::normal, class Dest = actor,
            class... Ts>
  void anon_send(const Dest& dest, Ts&&... xs) {
    static_assert(sizeof...(Ts) > 0, "no message to send");
    static_assert((detail::sendable<Ts> && ...),
                  "at least one type has no ID, "
                  "did you forgot to announce it via CAF_ADD_TYPE_ID?");
    using token = detail::type_list<detail::strip_and_convert_t<Ts>...>;
    static_assert(response_type_unbox<signatures_of_t<Dest>, token>::valid,
                  "receiver does not accept given message");
    auto self = dptr();
    detail::profiled_send(self, nullptr, dest, make_message_id(P), {},
                          self->context(), std::forward<Ts>(xs)...);
  }

  /// Sends a message at given time point (or immediately if `timeout` has
  /// passed already).
  template <message_priority P = message_priority::normal, class Dest = actor,
            class... Ts>
  disposable scheduled_send(const Dest& dest, actor_clock::time_point timeout,
                            Ts&&... xs) {
    static_assert(sizeof...(Ts) > 0, "no message to send");
    static_assert((detail::sendable<Ts> && ...),
                  "at least one type has no ID, "
                  "did you forgot to announce it via CAF_ADD_TYPE_ID?");
    detail::type_list<detail::strip_and_convert_t<Ts>...> args_token;
    type_check(dest, args_token);
    auto self = dptr();
    return detail::profiled_send(self, self->ctrl(), dest,
                                 self->system().clock(), timeout,
                                 make_message_id(P), std::forward<Ts>(xs)...);
  }

  /// Sends a message after a relative timeout.
  template <message_priority P = message_priority::normal, class Rep = int,
            class Period = std::ratio<1>, class Dest = actor, class... Ts>
  disposable delayed_send(const Dest& dest,
                          std::chrono::duration<Rep, Period> rel_timeout,
                          Ts&&... xs) {
    static_assert(sizeof...(Ts) > 0, "no message to send");
    static_assert((detail::sendable<Ts> && ...),
                  "at least one type has no ID, "
                  "did you forgot to announce it via CAF_ADD_TYPE_ID?");
    detail::type_list<detail::strip_and_convert_t<Ts>...> args_token;
    type_check(dest, args_token);
    auto self = dptr();
    auto& clock = self->system().clock();
    auto timeout = clock.now() + rel_timeout;
    return detail::profiled_send(self, self->ctrl(), dest, clock, timeout,
                                 make_message_id(P), std::forward<Ts>(xs)...);
  }

  template <message_priority P = message_priority::normal, class Dest = actor,
            class... Ts>
  disposable scheduled_anon_send(const Dest& dest,
                                 actor_clock::time_point timeout, Ts&&... xs) {
    static_assert(sizeof...(Ts) > 0, "no message to send");
    static_assert((detail::sendable<Ts> && ...),
                  "at least one type has no ID, "
                  "did you forgot to announce it via CAF_ADD_TYPE_ID?");
    detail::type_list<detail::strip_and_convert_t<Ts>...> args_token;
    type_check(dest, args_token);
    auto self = dptr();
    return detail::profiled_send(self, nullptr, dest, self->system().clock(),
                                 timeout, make_message_id(P),
                                 std::forward<Ts>(xs)...);
  }

  template <message_priority P = message_priority::normal, class Dest = actor,
            class Rep = int, class Period = std::ratio<1>, class... Ts>
  disposable delayed_anon_send(const Dest& dest,
                               std::chrono::duration<Rep, Period> rel_timeout,
                               Ts&&... xs) {
    static_assert(sizeof...(Ts) > 0, "no message to send");
    static_assert((detail::sendable<Ts> && ...),
                  "at least one type has no ID, "
                  "did you forgot to announce it via CAF_ADD_TYPE_ID?");
    detail::type_list<detail::strip_and_convert_t<Ts>...> args_token;
    type_check(dest, args_token);
    auto self = dptr();
    auto& clock = self->system().clock();
    auto timeout = clock.now() + rel_timeout;
    return detail::profiled_send(self, nullptr, dest, clock, timeout,
                                 make_message_id(P), std::forward<Ts>(xs)...);
  }

private:
  template <class Dest, class ArgTypes>
  static void type_check(const Dest&, ArgTypes) {
    static_assert(!statically_typed<Subtype>() || statically_typed<Dest>(),
                  "statically typed actors are only allowed to send() to other "
                  "statically typed actors; use anon_send() or request() when "
                  "communicating with dynamically typed actors");
    using rt = response_type_unbox<signatures_of_t<Dest>, ArgTypes>;
    static_assert(rt::valid, "receiver does not accept given message");
    // TODO: this only checks one way, we should check for loops
    static_assert(is_void_response<typename rt::type>::value
                    || response_type_unbox<signatures_of_t<Subtype>,
                                           typename rt::type>::valid,
                  "this actor does not accept the response message");
  }

  template <class T = Base>
  std::enable_if_t<std::is_base_of_v<abstract_actor, T>, Subtype*> dptr() {
    return static_cast<Subtype*>(this);
  }

  template <class T = Subtype, class = std::enable_if_t<
                                 std::is_base_of_v<typed_actor_view_base, T>>>
  typename T::pointer dptr() {
    return static_cast<Subtype*>(this)->internal_ptr();
  }
};

} // namespace caf::mixin
