// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/abstract_scheduled_actor.hpp"
#include "caf/async_mail.hpp"
#include "caf/detail/concepts.hpp"
#include "caf/event_based_fan_out_response_handle.hpp"
#include "caf/event_based_response_handle.hpp"
#include "caf/message.hpp"
#include "caf/policy/select_all.hpp"
#include "caf/policy/select_any.hpp"

namespace caf {

/// Provides a fluent interface for sending asynchronous messages to actors at a
/// specific point in time.
template <message_priority Priority, class Trait, class... Args>
class [[nodiscard]] event_based_scheduled_mail_t
  : public async_scheduled_mail_t<Priority, Trait, Args...> {
public:
  using super = async_scheduled_mail_t<Priority, Trait, Args...>;

  event_based_scheduled_mail_t(abstract_scheduled_actor* self,
                               message&& content,
                               actor_clock::time_point timeout)
    : super(self, std::move(content), timeout) {
    // nop
  }

  /// Sends the message to `receiver` as a request message and returns a handle
  /// for processing the response.
  /// @param receiver The actor that should receive the message.
  /// @param relative_timeout The maximum time to wait for a response.
  /// @param ref_tag Either `strong_ref` or `weak_ref`. When passing
  ///                `strong_ref`, the system will keep a strong reference to
  ///                the receiver until the message has been delivered.
  ///                Otherwise, the system will only keep a weak reference to
  ///                the receiver and the message will be dropped if the
  ///                receiver has been garbage collected in the meantime.
  /// @param self_ref_tag Either `strong_self_ref` or `weak_self_ref`. When
  ///                     passing `strong_self_ref`, the system will keep a
  ///                     strong reference to the sender until the message has
  ///                     been delivered. Otherwise, the system will only keep
  ///                     a weak reference to the sender and the message will be
  ///                     dropped if the sender has been garbage collected in
  ///                     the meantime.
  template <class Handle, class RefTag = strong_ref_t,
            class SelfRefTag = strong_self_ref_t>
  [[nodiscard]] auto request(const Handle& receiver, timespan relative_timeout,
                             RefTag ref_tag = {},
                             SelfRefTag self_ref_tag = {}) && {
    detail::send_type_check<none_t, Handle, Args...>();
    using response_type = response_type_t<typename Handle::signatures, Args...>;
    auto mid = self()->new_request_id(Priority);
    disposable in_flight_response;
    disposable in_flight_timeout;
    if (receiver) {
      auto& clock = self()->clock();
      if (relative_timeout != infinite) {
        in_flight_timeout = clock.schedule_message(
          nullptr, actor_cast(super::self_, weak_ref),
          super::timeout_ + relative_timeout, mid.response_id(),
          make_message(make_error(sec::request_timeout)));
      }
      in_flight_response
        = clock.schedule_message(actor_cast(super::self_, self_ref_tag),
                                 actor_cast(receiver, ref_tag), super::timeout_,
                                 mid, std::move(super::content_));

    } else {
      self()->enqueue(make_mailbox_element(self()->ctrl(), mid.response_id(),
                                           make_error(sec::invalid_request)),
                      self()->context());
    }
    using hdl_t = detail::event_based_delayed_response_handle_t<response_type>;
    return hdl_t{self(), mid.response_id(), std::move(in_flight_timeout),
                 std::move(in_flight_response)};
  }

  /// Sends `{xs...}` to each actor in the range `destinations` as a scheduled
  /// message. Response messages get combined into a single result according to
  /// the `Policy`.
  /// @tparam Policy Configures how individual response messages get combined by
  ///                the actor. The policy makes sure that the response handler
  ///                gets invoked at most once. In case of one or more errors,
  ///                the policy calls the error handler exactly once, with the
  ///                first error occurred.
  /// @tparam Container A container type for holding actor handles. Must provide
  ///                   the type alias `value_type` as well as the member
  ///                   functions `begin()`, `end()`, and `size()`.
  /// @param destinations A container holding handles to all destination actors.
  /// @param relative_timeout Maximum duration before dropping the request. The
  ///                         runtime system will return an error message in
  ///                         case the receiver does not respond in time.
  /// @param ref_tag Either `strong_ref` or `weak_ref`. When passing
  ///                `strong_ref`, the system will keep a strong reference to
  ///                the receiver until the message has been delivered.
  ///                Otherwise, the system will only keep a weak reference to
  ///                the receiver and the message will be dropped if the
  ///                receiver has been garbage collected in the meantime.
  /// @param self_ref_tag Either `strong_self_ref` or `weak_self_ref`. When
  ///                     passing `strong_self_ref`, the system will keep a
  ///                     strong reference to the sender until the message has
  ///                     been delivered. Otherwise, the system will only keep
  ///                     a weak reference to the sender and the message will be
  ///                     dropped if the sender has been garbage collected in
  ///                     the meantime.
  /// @returns A helper object that takes response handlers via `.await()`,
  ///          `.then()`, or converts to observables.
  /// @note The returned handle is actor-specific. Only the actor that called
  ///       `fan_out_request` can use it for setting response handlers.
  template <class Container, class Policy, class RefTag = strong_ref_t,
            class SelfRefTag = strong_self_ref_t>
  [[nodiscard]] auto
  fan_out_request(const Container& destinations, timespan relative_timeout,
                  Policy, RefTag ref_tag = {}, SelfRefTag self_ref_tag = {}) {
    static_assert(detail::one_of<Policy, policy::select_all_tag_t,
                                 policy::select_any_tag_t>,
                  "Allowed policies are select_all and select_any.");
    static_assert(is_ref_tag<RefTag>);
    static_assert(is_self_ref_tag<SelfRefTag>);
    using handle_type = typename Container::value_type;
    detail::send_type_check<none_t, handle_type, Args...>();
    std::vector<message_id> ids;
    ids.reserve(destinations.size());
    std::vector<disposable> pending_msgs;
    pending_msgs.reserve(destinations.size());
    std::vector<disposable> pending_requests;
    pending_requests.reserve(destinations.size());
    auto& clock = self()->clock();
    auto self_handle = actor_cast(super::self_, self_ref_tag);
    for (const auto& dest : destinations) {
      if (!dest)
        continue;
      auto req_id = self()->new_request_id(Priority);
      // Schedule the request message for delivery
      pending_requests.push_back(
        clock.schedule_message(self_handle, actor_cast(dest, ref_tag),
                               super::timeout_, req_id, super::content_));
      // Schedule timeout for response
      if (relative_timeout != infinite) {
        pending_msgs.emplace_back(clock.schedule_message(
          nullptr, actor_cast(super::self_, weak_ref),
          super::timeout_ + relative_timeout, req_id.response_id(),
          make_message(make_error(sec::request_timeout))));
      }
      ids.emplace_back(req_id.response_id());
    }
    if (ids.empty()) {
      auto req_id = self()->new_request_id(Priority);
      self()->enqueue(make_mailbox_element(self()->ctrl(), req_id.response_id(),
                                           make_error(sec::invalid_argument)),
                      self()->context());
      ids.emplace_back(req_id.response_id());
    }
    using response_type
      = response_type_t<typename handle_type::signatures,
                        detail::implicit_conversions_t<std::decay_t<Args>>...>;
    using result_type
      = event_based_fan_out_delayed_response_handle_t<Policy, response_type>;
    auto composite_timeout
      = disposable::make_composite(std::move(pending_msgs));
    auto composite_requests
      = disposable::make_composite(std::move(pending_requests));
    return result_type{self(), std::move(ids), std::move(composite_timeout),
                       std::move(composite_requests)};
  }

private:
  abstract_scheduled_actor* self() {
    return static_cast<abstract_scheduled_actor*>(super::self_);
  }
};

/// Provides a fluent interface for sending asynchronous messages to actors.
template <message_priority Priority, class Trait, class... Args>
class [[nodiscard]] event_based_mail_t
  : public async_mail_base_t<Priority, Trait, Args...> {
public:
  using super = async_mail_base_t<Priority, Trait, Args...>;

  event_based_mail_t(abstract_scheduled_actor* self, message&& content)
    : super(self, std::move(content)) {
    // nop
  }

  /// Tags the message as urgent, i.e., sends it with high priority.
  [[nodiscard]] auto urgent() &&
    requires(Priority == message_priority::normal)
  {
    using result_t = event_based_mail_t<message_priority::high, Trait, Args...>;
    return result_t{self(), std::move(super::content_)};
  }

  /// Schedules the message for delivery with an absolute timeout.
  [[nodiscard]] auto schedule(actor_clock::time_point timeout) && {
    using result_t = event_based_scheduled_mail_t<Priority, Trait, Args...>;
    return result_t{self(), std::move(super::content_), timeout};
  }

  /// Schedules the message for delivery with a relative timeout.
  [[nodiscard]] auto delay(actor_clock::duration_type timeout) && {
    using result_t = event_based_scheduled_mail_t<Priority, Trait, Args...>;
    return result_t{self(), std::move(super::content_),
                    self()->clock().now() + timeout};
  }

  /// Sends the message to `receiver` as a request message and returns a handle
  /// for processing the response.
  template <class Handle>
  [[nodiscard]] auto
  request(const Handle& receiver, timespan relative_timeout) && {
    detail::send_type_check<none_t, Handle, Args...>();
    using response_type = response_type_t<typename Handle::signatures, Args...>;
    auto mid = self()->new_request_id(Priority);
    disposable in_flight_timeout;
    if (receiver) {
      if (relative_timeout != infinite) {
        auto& clock = self()->clock();
        in_flight_timeout = clock.schedule_message(
          nullptr, actor_cast(super::self_, weak_ref),
          clock.now() + relative_timeout, mid.response_id(),
          make_message(make_error(sec::request_timeout)));
      }
      auto* ptr = actor_cast<abstract_actor*>(receiver);
      ptr->enqueue(make_mailbox_element(self()->ctrl(), mid,
                                        std::move(super::content_)),
                   self()->context());
    } else {
      self()->enqueue(make_mailbox_element(self()->ctrl(), mid.response_id(),
                                           make_error(sec::invalid_request)),
                      self()->context());
    }
    using hdl_t = detail::event_based_response_handle_t<response_type>;
    return hdl_t{self(), mid.response_id(), std::move(in_flight_timeout)};
  }

  /// Sends `{xs...}` to each actor in the range `destinations` as a synchronous
  /// message. Response messages get combined into a single result according to
  /// the `Policy`.
  /// @tparam Container A container type for holding actor handles. Must provide
  ///                   the type alias `value_type` as well as the member
  ///                   functions `begin()`, `end()`, and `size()`.
  /// @tparam Policy Configures how individual response messages get combined by
  ///                the actor. The policy makes sure that the response handler
  ///                gets invoked at most once. In case of one or more errors,
  ///                the policy calls the error handler exactly once, with the
  ///                first error occurred.
  /// @param destinations A container holding handles to all destination actors.
  /// @param timeout Maximum duration before dropping the request. The runtime
  ///                system will send an error message to the actor in case the
  ///                receiver does not respond in time.
  /// @returns A helper object that takes response handlers via `.await()`,
  ///          `.then()`, `.to_single()`, or `.to_observable()`.
  /// @note The returned handle is actor-specific. Only the actor that called
  ///       `fan_out_request` can use it for setting response handlers.
  template <class Container, class Policy>
  [[nodiscard]] auto
  fan_out_request(const Container& destinations, timespan timeout, Policy) {
    static_assert(detail::one_of<Policy, policy::select_all_tag_t,
                                 policy::select_any_tag_t>,
                  "Allowed policies are select_all and select_any.");
    using handle_type = typename Container::value_type;
    detail::send_type_check<none_t, handle_type, Args...>();
    std::vector<message_id> ids;
    ids.reserve(destinations.size());
    std::vector<disposable> pending_msgs;
    pending_msgs.reserve(destinations.size());
    for (const auto& dest : destinations) {
      if (!dest)
        continue;
      auto req_id = self()->new_request_id(Priority);
      dest->enqueue(make_mailbox_element(self()->ctrl(), req_id,
                                         super::content_),
                    self()->context());
      pending_msgs.emplace_back(
        self()->request_response_timeout(timeout, req_id));
      ids.emplace_back(req_id.response_id());
    }
    if (ids.empty()) {
      auto req_id = self()->new_request_id(Priority);
      self()->enqueue(make_mailbox_element(self()->ctrl(), req_id.response_id(),
                                           make_error(sec::invalid_argument)),
                      self()->context());
      ids.emplace_back(req_id.response_id());
    }
    using response_type
      = response_type_t<typename handle_type::signatures,
                        detail::implicit_conversions_t<std::decay_t<Args>>...>;
    using result_type
      = detail::event_based_fan_out_response_handle_t<Policy, response_type>;
    return result_type{self(), std::move(ids),
                       disposable::make_composite(std::move(pending_msgs))};
  }

private:
  abstract_scheduled_actor* self() {
    return static_cast<abstract_scheduled_actor*>(super::self_);
  }
};

/// Entry point for sending an event-based message to an actor.
template <class Trait, class... Args>
[[nodiscard]] auto
event_based_mail(Trait, abstract_scheduled_actor* self, Args&&... args) {
  using result_t = event_based_mail_t<message_priority::normal, Trait,
                                      detail::strip_and_convert_t<Args>...>;
  return result_t{self, make_message_nowrap(std::forward<Args>(args)...)};
}

} // namespace caf
