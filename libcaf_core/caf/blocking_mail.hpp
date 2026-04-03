// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/abstract_blocking_actor.hpp"
#include "caf/async_mail.hpp"
#include "caf/blocking_response_handle.hpp"
#include "caf/detail/current_actor.hpp"
#include "caf/message.hpp"

namespace caf {

/// Provides a fluent interface for sending asynchronous messages to actors at a
/// specific point in time.
template <message_priority Priority, class Trait, class... Args>
class [[nodiscard]] blocking_scheduled_mail_t {
public:
  blocking_scheduled_mail_t(abstract_blocking_actor* self, message&& content,
                            actor_clock::time_point timeout)
    : self_(self), content_(std::move(content)), timeout_(timeout) {
    // nop
  }

  blocking_scheduled_mail_t(const blocking_scheduled_mail_t&) = delete;

  blocking_scheduled_mail_t& operator=(const blocking_scheduled_mail_t&)
    = delete;

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
    detail::current_actor_guard guard{self_};
    detail::send_type_check<typename Trait::signatures, Handle, Args...>();
    using response_type = response_type_t<typename Handle::signatures, Args...>;
    auto mid = self_->new_request_id(Priority);
    disposable in_flight;
    if (receiver) {
      auto& clock = self_->clock();
      in_flight = clock.schedule_message(actor_cast(self_, self_ref_tag),
                                         actor_cast(receiver, ref_tag),
                                         timeout_, mid, std::move(content_));

    } else {
      self_->enqueue(make_mailbox_element({self_->ctrl(), add_ref},
                                          mid.response_id(),
                                          make_error(sec::invalid_request)),
                     self_->context());
    }
    using hdl_t = detail::blocking_delayed_response_handle_t<response_type>;
    return hdl_t{self_, mid.response_id(), relative_timeout,
                 std::move(in_flight)};
  }

  /// @copydoc async_scheduled_mail_t::send
  template <class Handle, class RefTag = strong_ref_t,
            class SelfRefTag = strong_self_ref_t>
  disposable send(const Handle& receiver, RefTag ref_tag = {},
                  SelfRefTag self_ref_tag = {}) && {
    detail::current_actor_guard guard{self_};
    return async_t{self_, std::move(content_), timeout_} //
      .send(receiver, ref_tag, self_ref_tag);
  }

  /// @copydoc async_scheduled_mail_t::delegate
  template <class Handle, class RefTag = strong_ref_t,
            class SelfRefTag = strong_self_ref_t>
  [[nodiscard]] auto delegate(const Handle& receiver, RefTag ref_tag = {},
                              SelfRefTag self_ref_tag = {}) && {
    detail::current_actor_guard guard{self_};
    return async_t{self_, std::move(content_), timeout_} //
      .delegate(receiver, ref_tag, self_ref_tag);
  }

private:
  using async_t = async_scheduled_mail_t<Priority, Trait, Args...>;

  abstract_blocking_actor* self_;
  message content_;
  actor_clock::time_point timeout_;
};

/// Provides a fluent interface for sending asynchronous messages to actors.
template <message_priority Priority, class Trait, class... Args>
class [[nodiscard]] blocking_mail_t {
public:
  blocking_mail_t(abstract_blocking_actor* self, message&& content)
    : self_(self), content_(std::move(content)) {
    // nop
  }

  blocking_mail_t(const blocking_mail_t&) = delete;

  blocking_mail_t& operator=(const blocking_mail_t&) = delete;

  /// Tags the message as urgent, i.e., sends it with high priority.
  [[nodiscard]] auto urgent() &&
    requires(Priority == message_priority::normal)
  {
    using result_t = blocking_mail_t<message_priority::high, Trait, Args...>;
    return result_t{self_, std::move(content_)};
  }

  /// Schedules the message for delivery with an absolute timeout.
  [[nodiscard]] auto schedule(actor_clock::time_point timeout) && {
    using result_t = blocking_scheduled_mail_t<Priority, Trait, Args...>;
    return result_t{self_, std::move(content_), timeout};
  }

  /// Schedules the message for delivery with a relative timeout.
  [[nodiscard]] auto delay(actor_clock::duration_type timeout) && {
    using clock = actor_clock::clock_type;
    using result_t = blocking_scheduled_mail_t<Priority, Trait, Args...>;
    return result_t{self_, std::move(content_), clock::now() + timeout};
  }

  /// @copydoc async_mail_base_t::send
  template <class Handle>
  void send(const Handle& receiver) && {
    detail::current_actor_guard guard{self_};
    async_t{self_, std::move(content_)}.send(receiver);
  }

  /// @copydoc async_mail_base_t::delegate
  template <class Handle>
  [[nodiscard]] auto delegate(const Handle& receiver) && {
    detail::current_actor_guard guard{self_};
    async_t{self_, std::move(content_)}.delegate(receiver);
  }

  /// Sends the message to `receiver` as a request message and returns a handle
  /// for processing the response.
  template <class Handle>
  [[nodiscard]] auto
  request(const Handle& receiver, timespan relative_timeout) && {
    detail::current_actor_guard guard{self_};
    detail::send_type_check<typename Trait::signatures, Handle, Args...>();
    using response_type = response_type_t<typename Handle::signatures, Args...>;
    auto mid = self_->new_request_id(Priority);
    if (receiver) {
      auto* ptr = actor_cast<abstract_actor*>(receiver);
      ptr->enqueue(make_mailbox_element({self_->ctrl(), add_ref}, mid,
                                        std::move(content_)),
                   self_->context());
    } else {
      self_->enqueue(make_mailbox_element({self_->ctrl(), add_ref},
                                          mid.response_id(),
                                          make_error(sec::invalid_request)),
                     self_->context());
    }
    using hdl_t = detail::blocking_response_handle_t<response_type>;
    return hdl_t{self_, mid.response_id(), relative_timeout};
  }

private:
  using async_t = async_mail_base_t<Priority, Trait, Args...>;

  abstract_blocking_actor* self_;
  message content_;
};

/// Entry point for sending an event-based message to an actor.
template <class Trait, class... Args>
[[nodiscard]] auto
blocking_mail(Trait, abstract_blocking_actor* self, Args&&... args) {
  using result_t = blocking_mail_t<message_priority::normal, Trait,
                                   detail::strip_and_convert_t<Args>...>;
  return result_t{self, make_message_nowrap(std::forward<Args>(args)...)};
}

} // namespace caf
