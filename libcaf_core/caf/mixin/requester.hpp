// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor.hpp"
#include "caf/detail/profiled_send.hpp"
#include "caf/disposable.hpp"
#include "caf/fwd.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/message.hpp"
#include "caf/message_id.hpp"
#include "caf/message_priority.hpp"
#include "caf/policy/single_response.hpp"
#include "caf/response_handle.hpp"
#include "caf/response_type.hpp"

#include <chrono>
#include <tuple>
#include <vector>

namespace caf::mixin {

/// A `requester` is an actor that supports
/// `self->request(...).{then|await|receive}`.
template <class Base, class Subtype>
class requester : public Base {
public:
  // -- member types -----------------------------------------------------------

  using extended_base = requester;

  // -- constructors, destructors, and assignment operators --------------------

  using Base::Base;

  // -- request ----------------------------------------------------------------

  /// Sends `{xs...}` as a synchronous message to `dest` with priority `mp`.
  /// @returns A handle identifying a future-like handle to the response.
  /// @warning The returned handle is actor specific and the response to the
  ///          sent message cannot be received by another actor.
  template <message_priority P = message_priority::normal, class Rep = int,
            class Period = std::ratio<1>, class Handle = actor, class... Ts>
  auto request(const Handle& dest, std::chrono::duration<Rep, Period> timeout,
               Ts&&... xs) {
    using namespace detail;
    static_assert(sizeof...(Ts) > 0, "no message to send");
    using token = type_list<implicit_conversions_t<std::decay_t<Ts>>...>;
    static_assert(response_type_unbox<signatures_of_t<Handle>, token>::valid,
                  "receiver does not accept given message");
    auto self = static_cast<Subtype*>(this);
    auto req_id = self->new_request_id(P);
    auto pending_msg = disposable{};
    if (dest) {
      detail::profiled_send(self, self->ctrl(), dest, req_id, self->context(),
                            std::forward<Ts>(xs)...);
      pending_msg = self->request_response_timeout(timeout, req_id);
    } else {
      self->enqueue(make_mailbox_element(self->ctrl(), req_id.response_id(),
                                         make_error(sec::invalid_argument)),
                    self->context());
      self->home_system().base_metrics().rejected_messages->inc();
    }
    using response_type
      = response_type_t<typename Handle::signatures,
                        detail::implicit_conversions_t<std::decay_t<Ts>>...>;
    using handle_type
      = response_handle<Subtype, policy::single_response<response_type>>;
    return handle_type{self, req_id.response_id(), std::move(pending_msg)};
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
            message_priority Prio = message_priority::normal, class Rep = int,
            class Period = std::ratio<1>, class Container, class... Ts>
  auto fan_out_request(const Container& destinations,
                       std::chrono::duration<Rep, Period> timeout, Ts&&... xs) {
    using handle_type = typename Container::value_type;
    using namespace detail;
    static_assert(sizeof...(Ts) > 0, "no message to send");
    using token = type_list<implicit_conversions_t<std::decay_t<Ts>>...>;
    static_assert(
      response_type_unbox<signatures_of_t<handle_type>, token>::valid,
      "receiver does not accept given message");
    auto dptr = static_cast<Subtype*>(this);
    std::vector<message_id> ids;
    ids.reserve(destinations.size());
    std::vector<disposable> pending_msgs;
    pending_msgs.reserve(destinations.size());
    for (const auto& dest : destinations) {
      if (!dest)
        continue;
      auto req_id = dptr->new_request_id(Prio);
      dest->enqueue(make_mailbox_element(dptr->ctrl(), req_id,
                                         std::forward<Ts>(xs)...),
                    dptr->context());
      pending_msgs.emplace_back(
        dptr->request_response_timeout(timeout, req_id));
      ids.emplace_back(req_id.response_id());
    }
    if (ids.empty()) {
      auto req_id = dptr->new_request_id(Prio);
      dptr->enqueue(make_mailbox_element(dptr->ctrl(), req_id.response_id(),
                                         make_error(sec::invalid_argument)),
                    dptr->context());
      ids.emplace_back(req_id.response_id());
    }
    using response_type
      = response_type_t<typename handle_type::signatures,
                        detail::implicit_conversions_t<std::decay_t<Ts>>...>;
    using result_type = response_handle<Subtype, MergePolicy<response_type>>;
    return result_type{dptr, std::move(ids),
                       disposable::make_composite(std::move(pending_msgs))};
  }
};

} // namespace caf::mixin
