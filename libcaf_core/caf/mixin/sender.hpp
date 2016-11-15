/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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
#include "caf/response_type.hpp"
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
    using detail::type_list;
    static_assert(sizeof...(Ts) > 0, "no message to send");
    using res_t = response_type<
                    signatures_of_t<Dest>,
                    detail::implicit_conversions_t<
                      typename std::decay<Ts>::type
                    >...>;
    static_assert(!statically_typed<Subtype>() || statically_typed<Dest>(),
                  "statically typed actors can only send() to other "
                  "statically typed actors; use anon_send() or request() when "
                  "communicating with dynamically typed actors");
    static_assert(res_t::valid, "receiver does not accept given message");
    static_assert(is_void_response<typename res_t::type>::value
                  || response_type_unbox<
                       signatures_of_t<Subtype>,
                       typename res_t::type
                     >::valid,
                  "this actor does not accept the response message");
    if (dest)
      dest->eq_impl(message_id::make(P), dptr()->ctrl(),
                    dptr()->context(), std::forward<Ts>(xs)...);
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
    static_assert(response_type_unbox<
                    signatures_of_t<Dest>,
                    token
                  >::valid,
                  "receiver does not accept given message");
    if (dest)
      dest->eq_impl(message_id::make(P), nullptr,
                    dptr()->context(), std::forward<Ts>(xs)...);
  }

  template <message_priority P = message_priority::normal,
            class Dest = actor, class... Ts>
  void delayed_send(const Dest& dest, const duration& rtime, Ts&&... xs) {
    using token =
      detail::type_list<
        typename detail::implicit_conversions<
          typename std::decay<Ts>::type
        >::type...>;
    static_assert(!statically_typed<Subtype>() || statically_typed<Dest>(),
                  "statically typed actors are only allowed to send() to other "
                  "statically typed actors; use anon_send() or request() when "
                  "communicating with dynamically typed actors");
    static_assert(response_type_unbox<
                    signatures_of_t<Dest>,
                    token
                  >::valid,
                  "receiver does not accept given message");
    // TODO: this only checks one way, we should check for loops
    static_assert(is_void_response<
                    typename response_type<
                      signatures_of_t<Dest>,
                      detail::implicit_conversions_t<
                        typename std::decay<Ts>::type
                      >...
                    >::type
                  >::value
                  ||  response_type_unbox<
                        signatures_of_t<Subtype>,
                        typename response_type<
                          signatures_of_t<Dest>,
                          detail::implicit_conversions_t<
                            typename std::decay<Ts>::type
                          >...
                        >::type
                      >::valid,
                  "this actor does not accept the response message");
    if (dest)
      dptr()->system().scheduler().delayed_send(
        rtime, dptr()->ctrl(), actor_cast<strong_actor_ptr>(dest),
        message_id::make(P), make_message(std::forward<Ts>(xs)...));
  }

  template <message_priority P = message_priority::normal,
            class Rep = int, class Period = std::ratio<1>,
            class Dest = actor, class... Ts>
  void delayed_send(const Dest& dest, std::chrono::duration<Rep, Period> rtime,
                    Ts&&... xs) {
    delayed_send(dest, duration{rtime}, std::forward<Ts>(xs)...);
  }

  template <message_priority P = message_priority::normal,
            class Source = actor, class Dest = actor, class... Ts>
  void delayed_anon_send(const Dest& dest, const duration& rtime, Ts&&... xs) {
    static_assert(sizeof...(Ts) > 0, "no message to send");
    using token =
      detail::type_list<
        typename detail::implicit_conversions<
          typename std::decay<Ts>::type
        >::type...>;
    static_assert(response_type_unbox<
                    signatures_of_t<Dest>,
                    token
                  >::valid,
                  "receiver does not accept given message");
    if (dest)
      dptr()->system().scheduler().delayed_send(
        rtime, nullptr, actor_cast<strong_actor_ptr>(dest), message_id::make(P),
        make_message(std::forward<Ts>(xs)...));
  }

  template <message_priority P = message_priority::normal,
            class Rep = int, class Period = std::ratio<1>,
            class Source = actor, class Dest = actor, class... Ts>
  void delayed_anon_send(const Dest& dest,
                         std::chrono::duration<Rep, Period> rtime,
                         Ts&&... xs) {
    delayed_anon_send(dest, duration{rtime}, std::forward<Ts>(xs)...);
  }

private:
  Subtype* dptr() {
    return static_cast<Subtype*>(this);
  }
};

} // namespace mixin
} // namespace caf

#endif // CAF_MIXIN_SENDER_HPP
