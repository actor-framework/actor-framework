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

#include <chrono>
#include <tuple>
#include <vector>

#include "caf/actor.hpp"
#include "caf/check_typed_input.hpp"
#include "caf/detail/profiled_send.hpp"
#include "caf/duration.hpp"
#include "caf/fwd.hpp"
#include "caf/message.hpp"
#include "caf/message_id.hpp"
#include "caf/message_priority.hpp"
#include "caf/policy/single_response.hpp"
#include "caf/response_handle.hpp"
#include "caf/response_type.hpp"

namespace caf::mixin {

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
  template <message_priority P = message_priority::normal, class Handle = actor,
            class... Ts>
  auto request(const Handle& dest, const duration& timeout, Ts&&... xs) {
    using namespace detail;
    static_assert(sizeof...(Ts) > 0, "no message to send");
    using token = type_list<implicit_conversions_t<decay_t<Ts>>...>;
    static_assert(response_type_unbox<signatures_of_t<Handle>, token>::valid,
                  "receiver does not accept given message");
    auto self = static_cast<Subtype*>(this);
    auto req_id = self->new_request_id(P);
    if (dest) {
      detail::profiled_send(self, self->ctrl(), dest, req_id, {},
                            self->context(), std::forward<Ts>(xs)...);
      self->request_response_timeout(timeout, req_id);
    } else {
      self->eq_impl(req_id.response_id(), self->ctrl(), self->context(),
                    make_error(sec::invalid_argument));
    }
    using response_type
      = response_type_t<typename Handle::signatures,
                        detail::implicit_conversions_t<detail::decay_t<Ts>>...>;
    using handle_type
      = response_handle<Subtype, policy::single_response<response_type>>;
    return handle_type{self, req_id.response_id()};
  }

  /// @copydoc request
  template <message_priority P = message_priority::normal, class Rep = int,
            class Period = std::ratio<1>, class Handle = actor, class... Ts>
  auto request(const Handle& dest, std::chrono::duration<Rep, Period> timeout,
               Ts&&... xs) {
    return request(dest, duration{timeout}, std::forward<Ts>(xs)...);
  }

  /// Sends `{xs...}` to each actor in the range `destinations` as a synchronous
  /// message. Response messages get combined into a single result according to
  /// the `MergePolicy`.
  /// @tparam MergePolicy Configures how individual response messages get
  ///                     combined by the actor. The policy makes sure that the
  ///                     response handler gets invoked at most once. In case of
  ///                     one or more errors, the policy calls the error handler
  ///                     exactly once, with the first error occurred.
  /// @tparam Prio Specifies the priority of the synchronous messages.
  /// @tparam Container A container type for holding actor handles. Must provide
  ///                   the type alias `value_type` as well as the member
  ///                   functions `begin()`, `end()`, and `size()`.
  /// @param destinations A container holding handles to all destination actors.
  /// @param timeout Maximum duration before dropping the request. The runtime
  ///                system will send an error message to the actor in case the
  ///                receiver does not respond in time.
  /// @returns A helper object that takes response handlers via `.await()`,
  ///          `.then()`, or `.receive()`.
  /// @note The returned handle is actor-specific. Only the actor that called
  ///       `request` can use it for setting response handlers.
  template <template <class> class MergePolicy,
            message_priority Prio = message_priority::normal, class Container,
            class... Ts>
  auto fan_out_request(const Container& destinations, const duration& timeout,
                       Ts&&... xs) {
    using handle_type = typename Container::value_type;
    using namespace detail;
    static_assert(sizeof...(Ts) > 0, "no message to send");
    using token = type_list<implicit_conversions_t<decay_t<Ts>>...>;
    static_assert(
      response_type_unbox<signatures_of_t<handle_type>, token>::valid,
      "receiver does not accept given message");
    auto dptr = static_cast<Subtype*>(this);
    std::vector<message_id> ids;
    ids.reserve(destinations.size());
    for (const auto& dest : destinations) {
      if (!dest)
        continue;
      auto req_id = dptr->new_request_id(Prio);
      dest->eq_impl(req_id, dptr->ctrl(), dptr->context(),
                    std::forward<Ts>(xs)...);
      dptr->request_response_timeout(timeout, req_id);
      ids.emplace_back(req_id.response_id());
    }
    if (ids.empty()) {
      auto req_id = dptr->new_request_id(Prio);
      dptr->eq_impl(req_id.response_id(), dptr->ctrl(), dptr->context(),
                    make_error(sec::invalid_argument));
      ids.emplace_back(req_id.response_id());
    }
    using response_type
      = response_type_t<typename handle_type::signatures,
                        detail::implicit_conversions_t<detail::decay_t<Ts>>...>;
    using result_type = response_handle<Subtype, MergePolicy<response_type>>;
    return result_type{dptr, std::move(ids)};
  }

  /// @copydoc fan_out_request
  template <template <class> class MergePolicy,
            message_priority Prio = message_priority::normal, class Rep,
            class Period, class Container, class... Ts>
  auto fan_out_request(const Container& destinations,
                       std::chrono::duration<Rep, Period> timeout, Ts&&... xs) {
    return request<MergePolicy, Prio>(destinations, duration{timeout},
                                      std::forward<Ts>(xs)...);
  }
};

} // namespace caf::mixin
