// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/abstract_actor.hpp"
#include "caf/actor_cast.hpp"
#include "caf/actor_clock.hpp"
#include "caf/actor_control_block.hpp"
#include "caf/detail/concepts.hpp"
#include "caf/detail/implicit_conversions.hpp"
#include "caf/detail/send_type_check.hpp"
#include "caf/disposable.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/message.hpp"
#include "caf/message_priority.hpp"
#include "caf/none.hpp"
#include "caf/policy/select_all.hpp"
#include "caf/policy/select_all_tag.hpp"
#include "caf/policy/select_any.hpp"
#include "caf/policy/select_any_tag.hpp"
#include "caf/ref.hpp"
#include "caf/response_type.hpp"
#include "caf/self_ref.hpp"

namespace caf {

/// Provides a fluent interface for sending asynchronous messages to actors at a
/// specific point in time.
template <class Trait, message_priority Priority, class... Args>
class scheduled_mailer {
public:
  using self_pointer = typename Trait::self_pointer;

  scheduled_mailer(self_pointer self, message&& content,
                   actor_clock::time_point timeout)
    : self_(self), content_(std::move(content)), timeout_(timeout) {
    // nop
  }

  scheduled_mailer(const scheduled_mailer&) = delete;

  scheduled_mailer& operator=(const scheduled_mailer&) = delete;

  /// Sends the message to `receiver`.
  /// @param receiver The actor that should receive the message.
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
  disposable send(const Handle& receiver, RefTag ref_tag = {},
                  SelfRefTag self_ref_tag = {}) && {
    static_assert(is_ref_tag<RefTag>);
    static_assert(is_self_ref_tag<SelfRefTag>);
    detail::send_type_check<signatures, Handle, Args...>();
    if (!receiver)
      return {};
    [[maybe_unused]] context_guard guard{self_};
    auto* ptr = actor_cast<abstract_actor*>(receiver);
    auto& clock = ptr->home_system().clock();
    return clock.schedule_message(actor_cast(self_, self_ref_tag),
                                  actor_cast(receiver, ref_tag), timeout_,
                                  make_message_id(Priority),
                                  std::move(content_));
  }

  /// Sends the message to `receiver`, using the message ID and sender from the
  /// currently processed message. Transfers the responsibility for responding
  /// to a request to `receiver`.
  /// @param receiver The actor that should receive the message.
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
    requires Trait::enable_delegate
  [[nodiscard]] auto delegate(const Handle& receiver, RefTag ref_tag = {},
                              SelfRefTag self_ref_tag = {}) && {
    static_assert(is_ref_tag<RefTag>);
    static_assert(is_self_ref_tag<SelfRefTag>);
    detail::send_type_check<signatures, Handle, Args...>();
    using delegated_t = delegated_response_type_t<Handle, Args...>;
    using result_t = std::pair<delegated_t, disposable>;
    [[maybe_unused]] context_guard guard{self_};
    if (!receiver) {
      self_->do_delegate_error();
      return result_t{delegated_t{}, disposable{}};
    }
    auto [mid, sender] = self_->template do_delegate<Priority>();
    auto* ptr = actor_cast<abstract_actor*>(receiver);
    auto& clock = ptr->home_system().clock();
    auto hdl = clock.schedule_message(actor_cast(sender, self_ref_tag),
                                      actor_cast(receiver, ref_tag), timeout_,
                                      mid, std::move(content_));
    return result_t{delegated_t{}, hdl};
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
    requires Trait::enable_request
  [[nodiscard]] auto request(const Handle& receiver, timespan relative_timeout,
                             RefTag ref_tag = {},
                             SelfRefTag self_ref_tag = {}) && {
    [[maybe_unused]] context_guard guard{self_};
    detail::send_type_check<signatures, Handle, Args...>();
    using response_type = response_type_t<typename Handle::signatures, Args...>;
    using response_handle =
      typename Trait::template delayed_response_handle<response_type>;
    auto mid = self_->new_request_id(Priority);
    if constexpr (response_handle::is_blocking) {
      disposable in_flight;
      if (receiver) {
        auto& clock = self_->clock();
        in_flight = clock.schedule_message(actor_cast(self_, self_ref_tag),
                                           actor_cast(receiver, ref_tag),
                                           timeout_, mid, std::move(content_));

      } else {
        self_->enqueue(make_mailbox_element(actor_cast(self_, strong_ref),
                                            mid.response_id(),
                                            make_error(sec::invalid_request)),
                       current_scheduler());
      }
      return response_handle{self_, mid.response_id(), relative_timeout,
                             std::move(in_flight)};
    } else {
      disposable in_flight_response;
      disposable in_flight_timeout;
      if (receiver) {
        auto& clock = self_->clock();
        if (relative_timeout != infinite) {
          in_flight_timeout = clock.schedule_message(
            nullptr, actor_cast(self_, weak_ref), timeout_ + relative_timeout,
            mid.response_id(), make_message(make_error(sec::request_timeout)));
        }
        in_flight_response
          = clock.schedule_message(actor_cast(self_, self_ref_tag),
                                   actor_cast(receiver, ref_tag), timeout_, mid,
                                   std::move(content_));

      } else {
        self_->enqueue(make_mailbox_element(actor_cast(self_, strong_ref),
                                            mid.response_id(),
                                            make_error(sec::invalid_request)),
                       current_scheduler());
      }
      return response_handle{self_, mid.response_id(),
                             std::move(in_flight_timeout),
                             std::move(in_flight_response)};
    }
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
    requires Trait::enable_fan_out_request
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
    auto& clock = self_->clock();
    auto self_handle = actor_cast(self_, self_ref_tag);
    for (const auto& dest : destinations) {
      if (!dest)
        continue;
      auto req_id = self_->new_request_id(Priority);
      // Schedule the request message for delivery
      pending_requests.push_back(clock.schedule_message(
        self_handle, actor_cast(dest, ref_tag), timeout_, req_id, content_));
      // Schedule timeout for response
      if (relative_timeout != infinite) {
        pending_msgs.emplace_back(clock.schedule_message(
          nullptr, actor_cast(self_, weak_ref), timeout_ + relative_timeout,
          req_id.response_id(),
          make_message(make_error(sec::request_timeout))));
      }
      ids.emplace_back(req_id.response_id());
    }
    if (ids.empty()) {
      auto req_id = self_->new_request_id(Priority);
      self_->enqueue(make_mailbox_element(actor_cast(self_, strong_ref),
                                          req_id.response_id(),
                                          make_error(sec::invalid_argument)),
                     current_scheduler());
      ids.emplace_back(req_id.response_id());
    }
    using response_type
      = response_type_t<typename handle_type::signatures,
                        detail::implicit_conversions_t<std::decay_t<Args>>...>;
    using result_type =
      typename Trait::template fan_out_delayed_response_handle<Policy,
                                                               response_type>;
    auto composite_timeout
      = disposable::make_composite(std::move(pending_msgs));
    auto composite_requests
      = disposable::make_composite(std::move(pending_requests));
    return result_type{self_, std::move(ids), std::move(composite_timeout),
                       std::move(composite_requests)};
  }

private:
  using signatures = typename Trait::signatures;

  using context_guard = typename Trait::context_guard;

  scheduler* current_scheduler() const noexcept {
    using actor_type = std::remove_pointer_t<self_pointer>;
    if constexpr (detail::has_context<actor_type>) {
      return self_->context();
    } else {
      return nullptr;
    }
  }

  self_pointer self_;
  message content_;
  actor_clock::time_point timeout_;
};

/// Provides a fluent interface for sending asynchronous messages to actors.
template <class Trait, message_priority Priority, class... Args>
class mailer {
public:
  using self_pointer = typename Trait::self_pointer;

  mailer(self_pointer self, message&& content)
    : self_(self), content_(std::move(content)) {
    // nop
  }

  mailer(const mailer&) = delete;

  mailer& operator=(const mailer&) = delete;

  /// Sends the message to `receiver`.
  template <class Handle>
  void send(const Handle& receiver) && {
    detail::send_type_check<signatures, Handle, Args...>();
    if (!receiver)
      return;
    [[maybe_unused]] context_guard guard{self_};
    auto* ptr = actor_cast<abstract_actor*>(receiver);
    ptr->enqueue(make_mailbox_element(actor_cast(self_, strong_ref),
                                      make_message_id(Priority),
                                      std::move(content_)),
                 current_scheduler());
  }

  /// Sends the message to `receiver`.
  template <class Handle>
    requires Trait::enable_delegate
  [[nodiscard]] auto delegate(const Handle& receiver) && {
    detail::send_type_check<none_t, Handle, Args...>();
    using result_t = delegated_response_type_t<Handle, Args...>;
    [[maybe_unused]] context_guard guard{self_};
    if (!receiver) {
      self_->do_delegate_error();
      return result_t{};
    }
    auto [mid, sender] = self_->template do_delegate<Priority>();
    auto* ptr = actor_cast<abstract_actor*>(receiver);
    ptr->enqueue(make_mailbox_element(std::move(sender), mid,
                                      std::move(content_)),
                 current_scheduler());
    return result_t{};
  }

  /// Sends the message to `receiver` as a request message and returns a handle
  /// for processing the response.
  template <class Handle>
    requires Trait::enable_request
  [[nodiscard]] auto
  request(const Handle& receiver, timespan relative_timeout) && {
    detail::send_type_check<none_t, Handle, Args...>();
    using response_type = response_type_t<typename Handle::signatures, Args...>;
    using response_handle =
      typename Trait::template response_handle<response_type>;
    [[maybe_unused]] context_guard guard{self_};
    auto mid = self_->new_request_id(Priority);
    if constexpr (response_handle::is_blocking) {
      if (receiver) {
        auto* ptr = actor_cast<abstract_actor*>(receiver);
        ptr->enqueue(make_mailbox_element(actor_cast(self_, strong_ref), mid,
                                          std::move(content_)),
                     current_scheduler());
      } else {
        self_->enqueue(make_mailbox_element(actor_cast(self_, strong_ref),
                                            mid.response_id(),
                                            make_error(sec::invalid_request)),
                       current_scheduler());
      }
      return response_handle{self_, mid.response_id(), relative_timeout};
    } else {
      disposable in_flight_timeout;
      if (receiver) {
        if (relative_timeout != infinite) {
          auto& clock = self_->clock();
          in_flight_timeout = clock.schedule_message(
            nullptr, actor_cast(self_, weak_ref),
            clock.now() + relative_timeout, mid.response_id(),
            make_message(make_error(sec::request_timeout)));
        }
        auto* ptr = actor_cast<abstract_actor*>(receiver);
        ptr->enqueue(make_mailbox_element(actor_cast(self_, strong_ref), mid,
                                          std::move(content_)),
                     current_scheduler());
      } else {
        self_->enqueue(make_mailbox_element(actor_cast(self_, strong_ref),
                                            mid.response_id(),
                                            make_error(sec::invalid_request)),
                       current_scheduler());
      }
      return response_handle{self_, mid.response_id(),
                             std::move(in_flight_timeout)};
    }
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
    requires Trait::enable_fan_out_request
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
      auto req_id = self_->new_request_id(Priority);
      dest->enqueue(make_mailbox_element(actor_cast(self_, strong_ref), req_id,
                                         content_),
                    current_scheduler());
      pending_msgs.emplace_back(
        self_->request_response_timeout(timeout, req_id));
      ids.emplace_back(req_id.response_id());
    }
    if (ids.empty()) {
      auto req_id = self_->new_request_id(Priority);
      self_->enqueue(make_mailbox_element(actor_cast(self_, strong_ref),
                                          req_id.response_id(),
                                          make_error(sec::invalid_argument)),
                     current_scheduler());
      ids.emplace_back(req_id.response_id());
    }
    using response_type
      = response_type_t<typename handle_type::signatures,
                        detail::implicit_conversions_t<std::decay_t<Args>>...>;
    using result_type =
      typename Trait::template fan_out_response_handle<Policy, response_type>;
    return result_type{self_, std::move(ids),
                       disposable::make_composite(std::move(pending_msgs))};
  }

  /// Tags the message as urgent, i.e., sends it with high priority.
  [[nodiscard]] auto urgent() &&
    requires(Priority == message_priority::normal)
  {
    using result_t = mailer<Trait, message_priority::high, Args...>;
    return result_t{self_, std::move(content_)};
  }

  /// Schedules the message for delivery with an absolute timeout.
  [[nodiscard]] auto schedule(actor_clock::time_point timeout) && {
    using result_t = scheduled_mailer<Trait, Priority, Args...>;
    return result_t{self_, std::move(content_), timeout};
  }

  /// Schedules the message for delivery with a relative timeout.
  [[nodiscard]] auto delay(actor_clock::duration_type timeout) && {
    using result_t = scheduled_mailer<Trait, Priority, Args...>;
    return result_t{self_, std::move(content_), now() + timeout};
  }

private:
  using signatures = typename Trait::signatures;

  using context_guard = typename Trait::context_guard;

  actor_clock::time_point now() const {
    using actor_type = std::remove_pointer_t<self_pointer>;
    if constexpr (std::is_same_v<actor_type, unit_t>) {
      return actor_clock::clock_type::now();
    } else {
      return self_->clock().now();
    }
  }

  scheduler* current_scheduler() const noexcept {
    using actor_type = std::remove_pointer_t<self_pointer>;
    if constexpr (detail::has_context<actor_type>) {
      return self_->context();
    } else {
      return nullptr;
    }
  }

  self_pointer self_;
  message content_;
};

/// Entry point for sending an asynchronous message to an actor.
template <class Trait, class... Args>
[[nodiscard]] auto
make_mailer(typename Trait::self_pointer self, Args&&... args) {
  using result_t = mailer<Trait, message_priority::normal,
                          detail::strip_and_convert_t<Args>...>;
  return result_t{self, make_message_nowrap(std::forward<Args>(args)...)};
}

} // namespace caf
