// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/abstract_actor.hpp"
#include "caf/actor_cast.hpp"
#include "caf/actor_clock.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/implicit_conversions.hpp"
#include "caf/detail/send_type_check.hpp"
#include "caf/disposable.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/message.hpp"
#include "caf/message_priority.hpp"
#include "caf/none.hpp"
#include "caf/ref.hpp"

namespace caf {

/// Provides a fluent interface for sending anonymous messages to actors at a
/// specific point in time.
template <message_priority Priority, class... Args>
class anon_scheduled_mail_t {
public:
  anon_scheduled_mail_t(message&& content, actor_clock::time_point timeout)
    : content_(std::move(content)), timeout_(timeout) {
    // nop
  }

  anon_scheduled_mail_t(const anon_scheduled_mail_t&) = delete;

  anon_scheduled_mail_t& operator=(const anon_scheduled_mail_t&) = delete;

  /// Sends the message to `receiver`.
  /// @param receiver The actor that should receive the message.
  /// @param ref_tag Either `strong_ref` or `weak_ref`. When passing
  ///                `strong_ref`, the system will keep a strong reference to
  ///                the receiver until the message has been delivered.
  ///                Otherwise, the system will only keep a weak reference to
  ///                the receiver and the message will be dropped if the
  ///                receiver has been garbage collected in the meantime.
  template <class Handle, class RefTag = strong_ref_t>
  disposable send(const Handle& receiver, RefTag ref_tag = {}) && {
    static_assert(is_ref_tag<RefTag>);
    detail::send_type_check<none_t, Handle, Args...>();
    if (!receiver)
      return {};
    auto* ptr = actor_cast<abstract_actor*>(receiver);
    auto& clock = ptr->home_system().clock();
    return clock.schedule_message(nullptr, actor_cast(receiver, ref_tag),
                                  timeout_, make_message_id(Priority),
                                  std::move(content_));
  }

private:
  message content_;
  actor_clock::time_point timeout_;
};

/// Provides a fluent interface for sending anonymous messages to actors.
template <message_priority Priority, class... Args>
class anon_mail_t {
public:
  explicit anon_mail_t(message&& content) : content_(std::move(content)) {
    // nop
  }

  anon_mail_t(const anon_mail_t&) = delete;

  anon_mail_t& operator=(const anon_mail_t&) = delete;

  /// Tags the message as urgent, i.e., sends it with high priority.
  template <message_priority P = Priority,
            class E = std::enable_if_t<P == message_priority::normal>>
  [[nodiscard]] auto urgent() && {
    using result_t = anon_mail_t<message_priority::high, Args...>;
    return result_t{std::move(content_)};
  }

  /// Schedules the message for delivery with an absolute timeout.
  [[nodiscard]] auto schedule(actor_clock::time_point timeout) && {
    using result_t = anon_scheduled_mail_t<Priority, Args...>;
    return result_t{std::move(content_), timeout};
  }

  /// Schedules the message for delivery with a relative timeout.
  [[nodiscard]] auto delay(actor_clock::duration_type timeout) && {
    using clock = actor_clock::clock_type;
    using result_t = anon_scheduled_mail_t<Priority, Args...>;
    return result_t{std::move(content_), clock::now() + timeout};
  }

  /// Sends the message to `receiver`.
  template <class Handle>
  void send(const Handle& receiver) && {
    detail::send_type_check<none_t, Handle, Args...>();
    if (!receiver)
      return;
    auto* ptr = actor_cast<abstract_actor*>(receiver);
    ptr->enqueue(make_mailbox_element(nullptr, make_message_id(Priority),
                                      std::move(content_)),
                 nullptr);
  }

private:
  message content_;
};

/// Entry point for sending an anonymous message to an actor.
template <class... Args>
[[nodiscard]] auto anon_mail(Args&&... args) {
  using result_t = anon_mail_t<message_priority::normal,
                               detail::strip_and_convert_t<Args>...>;
  return result_t{make_message(std::forward<Args>(args)...)};
}

} // namespace caf
