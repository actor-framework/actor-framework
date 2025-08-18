// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/abstract_scheduled_actor.hpp"
#include "caf/detail/concepts.hpp"
#include "caf/detail/response_type_check.hpp"
#include "caf/disposable.hpp"
#include "caf/flow/fwd.hpp"
#include "caf/fwd.hpp"
#include "caf/message_id.hpp"
#include "caf/policy/select_all.hpp"
#include "caf/policy/select_any.hpp"
#include "caf/type_list.hpp"

#include <type_traits>

namespace caf::policy {

struct select_all_tag_t {};
constexpr auto select_all_tag = select_all_tag_t{};

struct select_any_tag_t {};
constexpr auto select_any_tag = select_any_tag_t{};

} // namespace caf::policy

namespace caf::detail {

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

} // namespace caf::detail

namespace caf {

/// Holds state for a event-based response handles.
struct event_based_fan_out_response_handle_state {
  /// Points to the parent actor.
  abstract_scheduled_actor* self;

  /// Stores the IDs of the messages we are waiting for.
  std::vector<message_id> mids;

  /// Stores a handle to the in-flight timeout.
  disposable pending_timeout;
};

/// This helper class identifies an expected response message and enables
/// `request(...).then(...)`.
template <class Policy, class... Results>
class event_based_fan_out_response_handle {
public:
  // -- friends ----------------------------------------------------------------

  friend class scheduled_actor;

  // -- constructors, destructors, and assignment operators --------------------

  event_based_fan_out_response_handle(abstract_scheduled_actor* self,
                                      std::vector<message_id> mids,
                                      disposable pending_timeout)
    : state_{self, mids, std::move(pending_timeout)} {
    // nop
  }

  // -- then and await ---------------------------------------------------------

  template <class OnValue, class OnError>
  void await(OnValue on_value, OnError on_error) && {
    auto lg = log::core::trace("ids_ = {}", state_.mids);
    // TODO type checks
    // detail::response_type_check<OnValue, OnError, Results...>();
    behavior bhvr;
    if constexpr (std::same_as<Policy, policy::select_all_tag_t>) {
      bhvr = make_select_all_behavior(std::move(on_value), std::move(on_error));
    } else {
      bhvr = make_select_any_behavior(std::move(on_value), std::move(on_error));
    }
    for (const auto& mid : state_.mids)
      state_.self->add_awaited_response_handler(mid, bhvr,
                                                state_.pending_timeout);
  }

  template <class OnValue>
  void await(OnValue on_value) && {
    return std::move(*this).await(std::move(on_value),
                                  [self = state_.self](error& err) {
                                    self->call_error_handler(err);
                                  });
  }

  template <class OnValue, class OnError>
  void then(OnValue on_value, OnError on_error) && {
    auto lg = log::core::trace("ids_ = {}", state_.mids);
    // detail::response_type_check<OnValue, OnError, Results...>();
    // - no pending timeout in select_all, but there is in regular then.
    behavior bhvr;
    if constexpr (std::same_as<Policy, policy::select_all_tag_t>) {
      bhvr = make_select_all_behavior(std::move(on_value), std::move(on_error));
    } else {
      bhvr = make_select_any_behavior(std::move(on_value), std::move(on_error));
    }
    for (const auto& mid : state_.mids)
      state_.self->add_multiplexed_response_handler(mid, bhvr,
                                                    state_.pending_timeout);
  }

  template <class OnValue>
  void then(OnValue on_value) && {
    return std::move(*this).then(std::move(on_value),
                                 [self = state_.self](error& err) {
                                   self->call_error_handler(err);
                                 });
  }

  template <class... Ts>
  auto as_single() && {
    auto lg = log::core::trace("ids_ = {}", state_.mids);
    using value_type = detail::select_all_helper_value_t<Ts...>;
    if constexpr (std::same_as<Policy, policy::select_all_tag_t>) {
      auto cell = make_counted<flow::op::cell<std::vector<value_type>>>(
        state_.self->flow_context());
      auto bhvr = make_select_all_behavior(
        [cell, self = state_.self](std::vector<value_type> value) {
          cell->set_value(std::move(value));
          self->run_actions();
        },
        [cell, self = state_.self](error err) {
          cell->set_error(std::move(err));
          self->run_actions();
        });
      for (const auto& mid : state_.mids)
        state_.self->add_multiplexed_response_handler(mid, bhvr,
                                                      state_.pending_timeout);
      using val_t = typename decltype(cell)::value_type::output_type;
      return flow::single<val_t>{std::move(cell)}.as_observable();
    } else {
      auto cell
        = make_counted<flow::op::cell<value_type>>(state_.self->flow_context());
      auto bhvr = make_select_any_behavior(
        [cell, self = state_.self](value_type value) {
          cell->set_value(std::move(value));
          self->run_actions();
        },
        [cell, self = state_.self](error err) {
          cell->set_error(std::move(err));
          self->run_actions();
        });
      for (const auto& mid : state_.mids)
        state_.self->add_multiplexed_response_handler(mid, bhvr,
                                                      state_.pending_timeout);
      using val_t = typename decltype(cell)::value_type::output_type;
      return flow::single<val_t>{std::move(cell)};
    }
  }

  template <class T>
  auto as_observable() && {
    auto lg = log::core::trace("ids_ = {}", state_.mids);
    return std::move(*this).template as_single<T>().as_observable();
  }

private:
  /// Holds the state for the handle.
  event_based_fan_out_response_handle_state state_;

  template <class F, class OnError>
  behavior make_select_all_behavior(F&& f, OnError&& g) {
    using helper_type = detail::select_all_helper_t<std::decay_t<F>>;
    helper_type helper{state_.mids.size(), state_.pending_timeout,
                       std::forward<F>(f)};
    auto pending = helper.pending;
    auto error_handler = [pending{std::move(pending)},
                          timeouts{state_.pending_timeout},
                          g{std::forward<OnError>(g)}](error& err) mutable {
      auto lg = log::core::trace("pending = {}", *pending);
      if (*pending > 0) {
        timeouts.dispose();
        *pending = 0;
        g(err);
      }
    };
    return behavior{std::move(helper), std::move(error_handler)};
  }

  template <class F, class OnError>
  behavior make_select_any_behavior(F&& f, OnError&& g) {
    using factory = caf::detail::select_any_factory<std::decay_t<F>>;
    auto pending = std::make_shared<size_t>(state_.mids.size());
    auto result_handler = factory::make(pending, state_.pending_timeout,
                                        std::forward<F>(f));
    auto error_handler = [p{std::move(pending)},
                          timeouts{state_.pending_timeout},
                          g{std::forward<OnError>(g)}](error&) mutable {
      if (*p == 0) {
        // nop
      } else if (*p == 1) {
        timeouts.dispose();
        auto err = make_error(sec::all_requests_failed);
        g(err);
      } else {
        --*p;
      }
    };
    return {std::move(result_handler), std::move(error_handler)};
  }
};

} // namespace caf
