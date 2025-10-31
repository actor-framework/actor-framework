// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/abstract_blocking_actor.hpp"
#include "caf/actor_clock.hpp"
#include "caf/detail/expected_builder.hpp"
#include "caf/detail/response_type_check.hpp"
#include "caf/message_id.hpp"
#include "caf/policy/select_all.hpp"
#include "caf/policy/select_any.hpp"
#include "caf/sec.hpp"

#include <type_traits>

namespace caf::detail {

template <class Policy, class Result>
struct blocking_fan_out_response_handle_oracle;

template <class Policy>
struct blocking_fan_out_response_handle_oracle<Policy, message> {
  using type = blocking_fan_out_response_handle<Policy, message>;
};

template <class Policy>
struct blocking_fan_out_response_handle_oracle<Policy, type_list<void>> {
  using type = blocking_fan_out_response_handle<Policy>;
};

template <class Policy, class... Results>
struct blocking_fan_out_response_handle_oracle<Policy, type_list<Results...>> {
  using type = blocking_fan_out_response_handle<Policy, Results...>;
};

template <class Policy, class Result>
using blocking_fan_out_response_handle_t =
  typename blocking_fan_out_response_handle_oracle<Policy, Result>::type;

template <class Policy, class Result>
struct blocking_fan_out_delayed_response_handle_oracle;

template <class Policy>
struct blocking_fan_out_delayed_response_handle_oracle<Policy, message> {
  using type = blocking_fan_out_delayed_response_handle<Policy, message>;
};

template <class Policy>
struct blocking_fan_out_delayed_response_handle_oracle<Policy,
                                                       type_list<void>> {
  using type = blocking_fan_out_delayed_response_handle<Policy>;
};

template <class Policy, class... Results>
struct blocking_fan_out_delayed_response_handle_oracle<Policy,
                                                       type_list<Results...>> {
  using type = blocking_fan_out_delayed_response_handle<Policy, Results...>;
};

template <class Policy, class Result>
using blocking_fan_out_delayed_response_handle_t =
  typename blocking_fan_out_delayed_response_handle_oracle<Policy,
                                                           Result>::type;

} // namespace caf::detail

namespace caf {

/// Holds state for a blocking response handles.
struct blocking_fan_out_response_handle_state {
  /// Points to the parent actor.
  abstract_blocking_actor* self;

  /// Stores the ID of the message we are waiting for.
  std::vector<message_id> mids;

  /// Timeout for the response.
  disposable in_flight;

  /// Deadline for the response.
  actor_clock::time_point deadline;
};

/// This helper class identifies an expected response message and enables
/// `request(...).receive(...)`.
template <class Policy, class... Results>
class blocking_fan_out_response_handle {
public:
  blocking_fan_out_response_handle(abstract_blocking_actor* self,
                                   std::vector<message_id> mids,
                                   disposable in_flight,
                                   actor_clock::time_point deadline)
    : state_{self, std::move(mids), in_flight, deadline} {
    // nop
  }

  template <class OnValue, class OnError>
  void receive(OnValue on_value, OnError on_error) && {
    detail::fan_out_response_type_check<Policy, OnValue, OnError, Results...>();
    auto bhvr = behavior{};
    if constexpr (std::is_same_v<Policy, policy::select_all_tag_t>) {
      using helper_type = detail::select_all_helper_t<std::decay_t<OnValue>>;
      helper_type helper{state_.mids.size(), state_.in_flight,
                         std::forward<OnValue>(on_value)};
      auto pending = helper.pending;
      auto error_handler
        = [pending{helper.pending}, timeout{state_.in_flight},
           g{std::forward<OnError>(on_error)}](error& err) mutable {
            auto lg = log::core::trace("pending = {}", *pending);
            if (*pending > 0) {
              timeout.dispose();
              *pending = 0;
              g(err);
            }
          };
      bhvr = behavior{std::move(helper), std::move(error_handler)};
      for (const auto& mid : state_.mids) {
        auto now = std::chrono::steady_clock::now();
        auto remaining = state_.deadline > now
                           ? (state_.deadline - now)
                           : std::chrono::steady_clock::duration::zero();
        state_.self->do_receive(mid, bhvr, remaining);
      }
    } else {
      using helper = caf::detail::select_any_factory<std::decay_t<OnValue>>;
      auto pending = std::make_shared<size_t>(state_.mids.size());
      auto result_handler = helper::make(pending, state_.in_flight,
                                         std::forward<OnValue>(on_value));
      auto error_handler
        = [p{std::move(pending)}, timeout{state_.in_flight},
           g{std::forward<OnError>(on_error)}](error&) mutable {
            if (*p == 0) {
              // nop
            } else if (*p == 1) {
              timeout.dispose();
              auto err = make_error(sec::all_requests_failed);
              g(err);
            } else {
              --*p;
            }
          };
      bhvr = behavior{std::move(result_handler), std::move(error_handler)};
      for (const auto& mid : state_.mids) {
        auto now = std::chrono::steady_clock::now();
        auto remaining = state_.deadline > now
                           ? (state_.deadline - now)
                           : std::chrono::steady_clock::duration::zero();
        state_.self->do_receive(mid, bhvr, remaining);
      }
    }
  }

  auto receive() && {
    if constexpr (std::is_same_v<Policy, policy::select_all_tag_t>) {
      using value_type = detail::select_all_helper_value_t<Results...>;
      detail::expected_builder<std::vector<value_type>> bld;
      std::move(*this).receive(
        [&bld](std::vector<std::tuple<Results...>> args) {
          bld.set_value(std::move(args));
        },
        [&bld](error& err) { bld.set_error(std::move(err)); });
      return std::move(bld.result);
    } else {
      detail::expected_builder<Results...> bld;
      std::move(*this).receive(
        [&bld](Results... args) { bld.set_value(std::move(args)...); },
        [&bld](error& err) { bld.set_error(std::move(err)); });
      return std::move(bld.result);
    }
  }

private:
  /// Holds the state for the handle.
  blocking_fan_out_response_handle_state state_;
};

/// Similar to `blocking_response_handle`, but also holds the `disposable`
/// for the delayed request message.
template <class Policy, class... Results>
class blocking_fan_out_delayed_response_handle {
public:
  using decorated_type = blocking_fan_out_response_handle<Policy, Results...>;

  // -- constructors, destructors, and assignment operators --------------------

  blocking_fan_out_delayed_response_handle(abstract_blocking_actor* self,
                                           std::vector<message_id> mids,
                                           disposable in_flight,
                                           actor_clock::time_point deadline,
                                           disposable pending_request)
    : decorated(self, std::move(mids), in_flight, deadline),
      pending_request(std::move(pending_request)) {
    // nop
  }

  // -- receive ----------------------------------------------------------------

  template <class OnValue, class OnError>
  void receive(OnValue on_value, OnError on_error) && {
    std::move(decorated).receive(std::move(on_value), std::move(on_error));
  }

  auto receive() && {
    return std::move(decorated).receive();
  }

  /// The wrapped handle type.
  decorated_type decorated;

  /// Stores a handle to the in-flight request if the request messages was
  /// delayed/scheduled.
  disposable pending_request;
};

// tuple-like access for blocking_fan_out_delayed_response_handle

template <size_t I, class Policy, class... Results>
decltype(auto)
get(caf::blocking_fan_out_delayed_response_handle<Policy, Results...>& x) {
  if constexpr (I == 0) {
    return x.decorated;
  } else {
    static_assert(I == 1);
    return x.pending_request;
  }
}

template <size_t I, class Policy, class... Results>
decltype(auto) get(
  const caf::blocking_fan_out_delayed_response_handle<Policy, Results...>& x) {
  if constexpr (I == 0) {
    return x.decorated;
  } else {
    static_assert(I == 1);
    return x.pending_request;
  }
}

template <size_t I, class Policy, class... Results>
decltype(auto)
get(caf::blocking_fan_out_delayed_response_handle<Policy, Results...>&& x) {
  if constexpr (I == 0) {
    return std::move(x.decorated);
  } else {
    static_assert(I == 1);
    return std::move(x.pending_request);
  }
}

} // namespace caf

// enable structured bindings for blocking_fan_out_delayed_response_handle

namespace std {

template <class Policy, class... Results>
struct tuple_size<
  caf::blocking_fan_out_delayed_response_handle<Policy, Results...>> {
  static constexpr size_t value = 2;
};

template <class Policy, class... Results>
struct tuple_element<
  0, caf::blocking_fan_out_delayed_response_handle<Policy, Results...>> {
  using type = caf::blocking_fan_out_response_handle<Policy, Results...>;
};

template <class Policy, class... Results>
struct tuple_element<
  1, caf::blocking_fan_out_delayed_response_handle<Policy, Results...>> {
  using type = caf::disposable;
};

} // namespace std
