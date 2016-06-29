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

#ifndef CAF_MIXIN_SENDER_HPP
#define CAF_MIXIN_SENDER_HPP

#include <tuple>
#include <chrono>

#include "caf/fwd.hpp"
#include "caf/actor.hpp"
#include "caf/message.hpp"
#include "caf/duration.hpp"
#include "caf/response_handle.hpp"
#include "caf/message_priority.hpp"
#include "caf/check_typed_input.hpp"

namespace caf {
namespace mixin {

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

  /// Sends `{xs...}` as a synchronous message to `dest` with priority `mp`.
  /// @warning The returned handle is actor specific and the response to the
  ///          sent message cannot be received by another actor.
  /// @throws std::invalid_argument if `dest == invalid_actor`
  template <message_priority P = message_priority::normal,
            class Dest = actor, class... Ts>
  void send(const Dest& dest, Ts&&... xs) {
    static_assert(sizeof...(Ts) > 0, "no message to send");
    using token =
      detail::type_list<
        typename detail::implicit_conversions<
          typename std::decay<Ts>::type
        >::type...>;
    static_assert(! statically_typed<Subtype>() || statically_typed<Dest>(),
                  "statically typed actors can only send() to other "
                  "statically typed actors; use anon_send() or request() when "
                  "communicating with dynamically typed actors");
    static_assert(actor_accepts_message<
                    typename signatures_of<Dest>::type,
                    token
                  >::value,
                  "receiver does not accept given message");
    // TODO: this only checks one way, we should check for loops
    static_assert(is_void_response<
                    typename response_to<
                      typename signatures_of<Dest>::type,
                      token
                    >::type
                  >::value
                  ||  actor_accepts_message<
                        typename signatures_of<Subtype>::type,
                        typename response_to<
                          typename signatures_of<Dest>::type,
                          token
                        >::type
                      >::value,
                  "this actor does not accept the response message");
    dest->eq_impl(message_id::make(P), this->ctrl(),
                  this->context(), std::forward<Ts>(xs)...);
  }

  template <message_priority P = message_priority::normal,
            class Source = actor, class Dest = actor, class... Ts>
  void anon_send(const Dest& dest, Ts&&... xs) {
    static_assert(sizeof...(Ts) > 0, "no message to send");
    using token =
      detail::type_list<
        typename detail::implicit_conversions<
          typename std::decay<Ts>::type
        >::type...>;
    static_assert(actor_accepts_message<
                    typename signatures_of<Dest>::type,
                    token
                  >::value,
                  "receiver does not accept given message");
    dest->eq_impl(message_id::make(P), nullptr,
                  this->context(), std::forward<Ts>(xs)...);
  }

  template <message_priority P = message_priority::normal,
            class Dest = actor, class... Ts>
  void delayed_send(const Dest& dest, const duration& rtime, Ts&&... xs) {
    using token =
      detail::type_list<
        typename detail::implicit_conversions<
          typename std::decay<Ts>::type
        >::type...>;
    static_assert(! statically_typed<Subtype>() || statically_typed<Dest>(),
                  "statically typed actors are only allowed to send() to other "
                  "statically typed actors; use anon_send() or request() when "
                  "communicating with dynamically typed actors");
    static_assert(actor_accepts_message<
                    typename signatures_of<Dest>::type,
                    token
                  >::value,
                  "receiver does not accept given message");
    // TODO: this only checks one way, we should check for loops
    static_assert(is_void_response<
                    typename response_to<
                      typename signatures_of<Dest>::type,
                      token
                    >::type
                  >::value
                  ||  actor_accepts_message<
                        typename signatures_of<Subtype>::type,
                        typename response_to<
                          typename signatures_of<Dest>::type,
                          token
                        >::type
                      >::value,
                  "this actor does not accept the response message");
    this->system().scheduler().delayed_send(
      rtime, this->ctrl(), actor_cast<strong_actor_ptr>(dest),
      message_id::make(P), make_message(std::forward<Ts>(xs)...));
  }
};

} // namespace mixin
} // namespace caf

#endif // CAF_MIXIN_SENDER_HPP
