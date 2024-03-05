// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/abstract_scheduled_actor.hpp"
#include "caf/async_mail.hpp"
#include "caf/event_based_response_handle.hpp"
#include "caf/message.hpp"

namespace caf {

/// Provides a fluent interface for sending asynchronous messages to actors at a
/// specific point in time.
template <message_priority Priority, class Trait, class... Args>
class event_based_scheduled_mail_t
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

private:
  abstract_scheduled_actor* self() {
    return static_cast<abstract_scheduled_actor*>(super::self_);
  }
};

/// Provides a fluent interface for sending asynchronous messages to actors.
template <message_priority Priority, class Trait, class... Args>
class event_based_mail_t : public async_mail_base_t<Priority, Trait, Args...> {
public:
  using super = async_mail_base_t<Priority, Trait, Args...>;

  event_based_mail_t(abstract_scheduled_actor* self, message&& content)
    : super(self, std::move(content)) {
    // nop
  }

  /// Tags the message as urgent, i.e., sends it with high priority.
  template <message_priority P = Priority,
            class E = std::enable_if_t<P == message_priority::normal>>
  [[nodiscard]] auto urgent() && {
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
