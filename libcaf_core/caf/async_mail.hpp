// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/abstract_actor.hpp"
#include "caf/actor_cast.hpp"
#include "caf/actor_clock.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/implicit_conversions.hpp"
#include "caf/detail/send_type_check.hpp"
#include "caf/disposable.hpp"
#include "caf/local_actor.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/message.hpp"
#include "caf/message_priority.hpp"
#include "caf/none.hpp"
#include "caf/ref.hpp"
#include "caf/response_type.hpp"
#include "caf/self_ref.hpp"

namespace caf {

/// Provides a fluent interface for sending asynchronous messages to actors at a
/// specific point in time.
template <message_priority Priority, class Trait, class... Args>
class async_scheduled_mail_t {
public:
  async_scheduled_mail_t(local_actor* self, message&& content,
                         actor_clock::time_point timeout)
    : self_(self), content_(std::move(content)), timeout_(timeout) {
    // nop
  }

  async_scheduled_mail_t(const async_scheduled_mail_t&) = delete;

  async_scheduled_mail_t& operator=(const async_scheduled_mail_t&) = delete;

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
    detail::send_type_check<typename Trait::signatures, Handle, Args...>();
    if (!receiver)
      return {};
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
  [[nodiscard]] auto delegate(const Handle& receiver, RefTag ref_tag = {},
                              SelfRefTag self_ref_tag = {}) && {
    static_assert(is_ref_tag<RefTag>);
    static_assert(is_self_ref_tag<SelfRefTag>);
    detail::send_type_check<typename Trait::signatures, Handle, Args...>();
    using delegated_t = delegated_response_type_t<Handle, Args...>;
    using result_t = std::pair<delegated_t, disposable>;
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

protected:
  local_actor* self_;
  message content_;
  actor_clock::time_point timeout_;
};

/// Provides a fluent interface for sending asynchronous messages to actors.
template <message_priority Priority, class Trait, class... Args>
class async_mail_base_t {
public:
  async_mail_base_t(local_actor* self, message&& content)
    : self_(self), content_(std::move(content)) {
    // nop
  }

  async_mail_base_t(const async_mail_base_t&) = delete;

  async_mail_base_t& operator=(const async_mail_base_t&) = delete;

  /// Sends the message to `receiver`.
  template <class Handle>
  void send(const Handle& receiver) && {
    detail::send_type_check<typename Trait::signatures, Handle, Args...>();
    if (!receiver)
      return;
    auto* ptr = actor_cast<abstract_actor*>(receiver);
    ptr->enqueue(make_mailbox_element(self_->ctrl(), make_message_id(Priority),
                                      std::move(content_)),
                 self_->context());
  }

  /// Sends the message to `receiver`.
  template <class Handle>
  [[nodiscard]] auto delegate(const Handle& receiver) && {
    detail::send_type_check<none_t, Handle, Args...>();
    using result_t = delegated_response_type_t<Handle, Args...>;
    if (!receiver) {
      self_->do_delegate_error();
      return result_t{};
    }
    auto [mid, sender] = self_->template do_delegate<Priority>();
    auto* ptr = actor_cast<abstract_actor*>(receiver);
    ptr->enqueue(make_mailbox_element(std::move(sender), mid,
                                      std::move(content_)),
                 self_->context());
    return result_t{};
  }

protected:
  local_actor* self_;
  message content_;
};

/// Provides a fluent interface for sending asynchronous messages to actors.
template <message_priority Priority, class Trait, class... Args>
class async_mail_t : public async_mail_base_t<Priority, Trait, Args...> {
public:
  using super = async_mail_base_t<Priority, Trait, Args...>;

  using super::super;

  /// Tags the message as urgent, i.e., sends it with high priority.
  template <message_priority P = Priority,
            class E = std::enable_if_t<P == message_priority::normal>>
  [[nodiscard]] auto urgent() && {
    using result_t = async_mail_t<message_priority::high, Trait, Args...>;
    return result_t{super::self_, std::move(super::content_)};
  }

  /// Schedules the message for delivery with an absolute timeout.
  [[nodiscard]] auto schedule(actor_clock::time_point timeout) && {
    using result_t = async_scheduled_mail_t<Priority, Trait, Args...>;
    return result_t{super::self_, std::move(super::content_), timeout};
  }

  /// Schedules the message for delivery with a relative timeout.
  [[nodiscard]] auto delay(actor_clock::duration_type timeout) && {
    using clock = actor_clock::clock_type;
    using result_t = async_scheduled_mail_t<Priority, Trait, Args...>;
    return result_t{super::self_, std::move(super::content_),
                    clock::now() + timeout};
  }
};

/// Entry point for sending an asynchronous message to an actor.
template <class Trait, class... Args>
[[nodiscard]] auto async_mail(Trait, local_actor* self, Args&&... args) {
  using result_t = async_mail_t<message_priority::normal, Trait,
                                detail::strip_and_convert_t<Args>...>;
  return result_t{self, make_message_nowrap(std::forward<Args>(args)...)};
}

} // namespace caf
