// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor_clock.hpp"
#include "caf/add_ref.hpp"
#include "caf/detail/current_actor.hpp"
#include "caf/fwd.hpp"
#include "caf/mailbox_element.hpp"

namespace caf::detail {

template <class Result>
struct blocking_response_handle_oracle;

template <>
struct blocking_response_handle_oracle<message> {
  using type = blocking_response_handle<message>;
};

template <>
struct blocking_response_handle_oracle<type_list<void>> {
  using type = blocking_response_handle<>;
};

template <class... Results>
struct blocking_response_handle_oracle<type_list<Results...>> {
  using type = blocking_response_handle<Results...>;
};

template <class Result>
using blocking_response_handle_t =
  typename blocking_response_handle_oracle<Result>::type;

template <class Result>
struct blocking_delayed_response_handle_oracle;

template <>
struct blocking_delayed_response_handle_oracle<message> {
  using type = blocking_delayed_response_handle<message>;
};

template <>
struct blocking_delayed_response_handle_oracle<type_list<void>> {
  using type = blocking_delayed_response_handle<>;
};

template <class... Results>
struct blocking_delayed_response_handle_oracle<type_list<Results...>> {
  using type = blocking_delayed_response_handle<Results...>;
};

template <class Result>
using blocking_delayed_response_handle_t =
  typename blocking_delayed_response_handle_oracle<Result>::type;

} // namespace caf::detail

namespace caf::policy {

/// Policy for configuring @ref mailer for blocking actors.
struct blocking_requester {
  /// The type of the self pointer that will be passed to @ref mailer.
  using self_pointer = abstract_blocking_actor*;

  /// Messaging interface for the sending actor.
  using signatures = none_t;

  /// The guard type for setting and resetting thread-local context.
  using context_guard = detail::current_actor_guard;

  /// Whether to enable the `delegate` method.
  static constexpr bool enable_delegate = true;

  /// Whether to enable the `request` method.
  static constexpr bool enable_request = true;

  /// Whether to enable the `fan_out_request` method.
  static constexpr bool enable_fan_out_request = false;

  /// Maps a response type to a blocking response handle.
  template <class T>
  using response_handle = detail::blocking_response_handle_t<T>;

  /// Maps a response type to a blocking delayed response handle.
  template <class T>
  using delayed_response_handle = detail::blocking_delayed_response_handle_t<T>;

  template <class RefTag>
  static typename RefTag::handle_type ref(self_pointer self, RefTag) {
    return {self->ctrl(), add_ref};
  }

  static scheduler* context(self_pointer self) noexcept {
    return self->context();
  }

  static void delegate_error(self_pointer self) {
    self->do_delegate_error();
  }

  template <message_priority Priority>
  static auto delegate(self_pointer self) {
    return self->template do_delegate<Priority>();
  }

  static message_id new_request_id(self_pointer self,
                                   message_priority mp) noexcept {
    return self->new_request_id(mp);
  }

  static void send(self_pointer self, mailbox_element_ptr&& what,
                   scheduler* sched) {
    self->enqueue(std::move(what), sched);
  }

  static actor_clock& clock(self_pointer self) {
    return self->clock();
  }

  static disposable request_response_timeout(self_pointer self, timespan d,
                                             message_id mid) {
    return self->request_response_timeout(d, mid);
  }
};

} // namespace caf::policy
