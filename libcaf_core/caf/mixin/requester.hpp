/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
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

#include <tuple>
#include <chrono>

#include "caf/fwd.hpp"
#include "caf/actor.hpp"
#include "caf/message.hpp"
#include "caf/duration.hpp"
#include "caf/message_id.hpp"
#include "caf/response_type.hpp"
#include "caf/response_handle.hpp"
#include "caf/message_priority.hpp"
#include "caf/check_typed_input.hpp"

namespace caf {
namespace mixin {

template <class T>
struct is_blocking_requester : std::false_type { };

/// A `requester` is an actor that supports
/// `self->request(...).{then|await|receive}`.
template <class Base, class Subtype>
class requester : public Base {
public:
  // -- member types -----------------------------------------------------------

  using extended_base = requester;

  // -- constructors, destructors, and assignment operators --------------------

  template <class... Ts>
  requester(Ts&&... xs) : Base(std::forward<Ts>(xs)...) {
    // nop
  }

  // -- request ----------------------------------------------------------------

  /// Sends `{xs...}` as a synchronous message to `dest` with priority `mp`.
  /// @returns A handle identifying a future-like handle to the response.
  /// @warning The returned handle is actor specific and the response to the
  ///          sent message cannot be received by another actor.
  template <message_priority P = message_priority::normal,
            class Handle = actor, class... Ts>
  response_handle<Subtype,
                  response_type_t<
                    typename Handle::signatures,
                    typename detail::implicit_conversions<
                      typename std::decay<Ts>::type
                    >::type...>,
                  is_blocking_requester<Subtype>::value>
  request(const Handle& dest, const duration& timeout, Ts&&... xs) {
    static_assert(sizeof...(Ts) > 0, "no message to send");
    using token =
      detail::type_list<
        typename detail::implicit_conversions<
          typename std::decay<Ts>::type
        >::type...>;
    static_assert(response_type_unbox<signatures_of_t<Handle>, token>::valid,
                  "receiver does not accept given message");
    auto dptr = static_cast<Subtype*>(this);
    auto req_id = dptr->new_request_id(P);
    if (dest) {
      dest->eq_impl(req_id, dptr->ctrl(), dptr->context(),
                    std::forward<Ts>(xs)...);
      dptr->request_response_timeout(timeout, req_id);
    } else {
      dptr->eq_impl(req_id.response_id(), dptr->ctrl(), dptr->context(),
                    make_error(sec::invalid_argument));
    }
    return {req_id.response_id(), dptr};
   }

  /// Sends `{xs...}` as a synchronous message to `dest` with priority `mp`.
  /// @returns A handle identifying a future-like handle to the response.
  /// @warning The returned handle is actor specific and the response to the
  ///          sent message cannot be received by another actor.
  template <message_priority P = message_priority::normal,
            class Rep = int, class Period = std::ratio<1>,
            class Handle = actor, class... Ts>
  response_handle<Subtype,
                  response_type_t<
                    typename Handle::signatures,
                    typename detail::implicit_conversions<
                      typename std::decay<Ts>::type
                    >::type...>,
                  is_blocking_requester<Subtype>::value>
  request(const Handle& dest, std::chrono::duration<Rep, Period> timeout,
          Ts&&... xs) {
    return request(dest, duration{timeout}, std::forward<Ts>(xs)...);
  }
};

} // namespace mixin
} // namespace caf

