// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/abstract_actor.hpp"
#include "caf/actor_cast.hpp"
#include "caf/actor_clock.hpp"
#include "caf/detail/concepts.hpp"
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
    context_guard guard{self_};
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
    context_guard guard{self_};
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

private:
  using signatures = typename Trait::signatures;

  using context_guard = typename Trait::context_guard;

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
    context_guard guard{self_};
    auto* ptr = actor_cast<abstract_actor*>(receiver);
    ptr->enqueue(make_mailbox_element({self_->ctrl(), add_ref},
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
    context_guard guard{self_};
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
    using clock = actor_clock::clock_type;
    using result_t = scheduled_mailer<Trait, Priority, Args...>;
    return result_t{self_, std::move(content_), clock::now() + timeout};
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
