// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/abstract_scheduled_actor.hpp"
#include "caf/actor_clock.hpp"
#include "caf/add_ref.hpp"
#include "caf/fwd.hpp"
#include "caf/mailbox_element.hpp"

namespace caf::detail {

template <class Result>
struct event_based_response_handle_oracle;

template <>
struct event_based_response_handle_oracle<message> {
  using type = event_based_response_handle<message>;
};

template <>
struct event_based_response_handle_oracle<type_list<void>> {
  using type = event_based_response_handle<>;
};

template <class... Results>
struct event_based_response_handle_oracle<type_list<Results...>> {
  using type = event_based_response_handle<Results...>;
};

template <class Result>
using event_based_response_handle_t =
  typename event_based_response_handle_oracle<Result>::type;

template <class Result>
struct event_based_delayed_response_handle_oracle;

template <>
struct event_based_delayed_response_handle_oracle<message> {
  using type = event_based_delayed_response_handle<message>;
};

template <>
struct event_based_delayed_response_handle_oracle<type_list<void>> {
  using type = event_based_delayed_response_handle<>;
};

template <class... Results>
struct event_based_delayed_response_handle_oracle<type_list<Results...>> {
  using type = event_based_delayed_response_handle<Results...>;
};

template <class Result>
using event_based_delayed_response_handle_t =
  typename event_based_delayed_response_handle_oracle<Result>::type;

template <class Policy, class Result>
struct event_based_fan_out_response_handle_oracle;

template <class Policy>
struct event_based_fan_out_response_handle_oracle<Policy, message> {
  using type = event_based_fan_out_response_handle<Policy, message>;
};

template <class Policy>
struct event_based_fan_out_response_handle_oracle<Policy, type_list<void>> {
  using type = event_based_fan_out_response_handle<Policy>;
};

template <class Policy, class... Results>
struct event_based_fan_out_response_handle_oracle<Policy,
                                                  type_list<Results...>> {
  using type = event_based_fan_out_response_handle<Policy, Results...>;
};

template <class Policy, class Result>
using event_based_fan_out_response_handle_t =
  typename event_based_fan_out_response_handle_oracle<Policy, Result>::type;

// Type aliases for event_based_fan_out_delayed_response_handle
template <class Policy, class Result>
struct event_based_fan_out_delayed_response_handle_oracle;

template <class Policy>
struct event_based_fan_out_delayed_response_handle_oracle<Policy, message> {
  using type = event_based_fan_out_delayed_response_handle<Policy, message>;
};

template <class Policy>
struct event_based_fan_out_delayed_response_handle_oracle<Policy,
                                                          type_list<void>> {
  using type = event_based_fan_out_delayed_response_handle<Policy>;
};

template <class Policy, class... Results>
struct event_based_fan_out_delayed_response_handle_oracle<
  Policy, type_list<Results...>> {
  using type = event_based_fan_out_delayed_response_handle<Policy, Results...>;
};

template <class Policy, class Result>
using event_based_fan_out_delayed_response_handle_t =
  typename event_based_fan_out_delayed_response_handle_oracle<Policy,
                                                              Result>::type;

} // namespace caf::detail

namespace caf::policy {

/// Policy for configuring @ref mailer for event-based actors.
struct event_based_requester {
  /// The type of the self pointer that will be passed to @ref mailer.
  using self_pointer = abstract_scheduled_actor*;

  /// Messaging interface for the sending actor.
  using signatures = none_t;

  /// The guard type for setting and resetting thread-local context.
  using context_guard = unit_t; // no guard needed for event-based actors

  /// Whether to enable the `delegate` method.
  static constexpr bool enable_delegate = true;

  /// Whether to enable the `request` method.
  static constexpr bool enable_request = true;

  /// Whether to enable the `fan_out_request` method.
  static constexpr bool enable_fan_out_request = true;

  /// Maps a response type to a event-based response handle.
  template <class T>
  using response_handle = detail::event_based_response_handle_t<T>;

  /// Maps a response type to a event-based delayed response handle.
  template <class T>
  using delayed_response_handle
    = detail::event_based_delayed_response_handle_t<T>;

  /// Maps a response type to a event-based fan-out response handle.
  template <class Policy, class T>
  using fan_out_response_handle
    = detail::event_based_fan_out_response_handle_t<Policy, T>;

  /// Maps a response type to a event-based fan-out delayed response handle.
  template <class Policy, class T>
  using fan_out_delayed_response_handle
    = detail::event_based_fan_out_delayed_response_handle_t<Policy, T>;

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
