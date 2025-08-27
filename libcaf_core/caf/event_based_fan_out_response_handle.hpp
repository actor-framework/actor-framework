// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/abstract_scheduled_actor.hpp"
#include "caf/detail/response_type_check.hpp"
#include "caf/disposable.hpp"
#include "caf/flow/fwd.hpp"
#include "caf/fwd.hpp"
#include "caf/message_id.hpp"
#include "caf/policy/select_all.hpp"
#include "caf/policy/select_all_tag.hpp"
#include "caf/policy/select_any.hpp"
#include "caf/policy/select_any_tag.hpp"
#include "caf/type_list.hpp"

#include <type_traits>

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

template <class, class...>
struct fan_out_response_to_flow_cell_helper;

template <class Policy>
struct fan_out_response_to_flow_cell_helper<Policy> {
  static auto make_behavior(abstract_scheduled_actor* self,
                            flow::coordinator* coord) {
    auto cell = make_counted<flow::op::cell<unit_t>>(coord);
    return std::tuple{std::move(cell),
                      [self, cell] {
                        cell->set_value(unit);
                        self->run_actions();
                      },
                      [self, cell](error err) {
                        cell->set_error(std::move(err));
                        self->run_actions();
                      }};
  }
};

template <class Policy, class T>
struct fan_out_response_to_flow_cell_helper<Policy, T> {
  static auto make_behavior(abstract_scheduled_actor* self,
                            flow::coordinator* coord) {
    using value_type
      = std::conditional_t<std::is_same_v<Policy, policy::select_all_tag_t>,
                           std::vector<T>, T>;
    auto cell = make_counted<flow::op::cell<value_type>>(coord);
    return std::tuple(
      std::move(cell),
      [self, cell](value_type value) {
        cell->set_value(std::move(value));
        self->run_actions();
      },
      [self, cell](error err) {
        cell->set_error(std::move(err));
        self->run_actions();
      });
  }
};

template <class Policy, class T1, class T2, class... Ts>
struct fan_out_response_to_flow_cell_helper<Policy, T1, T2, Ts...> {
  static auto make_behavior(abstract_scheduled_actor* self,
                            flow::coordinator* coord) {
    using tuple_t = cow_tuple<T1, T2, Ts...>;
    using value_type
      = std::conditional_t<std::is_same_v<Policy, policy::select_all_tag_t>,
                           std::vector<tuple_t>, tuple_t>;
    auto cell = make_counted<flow::op::cell<value_type>>(coord);
    return std::tuple(
      std::move(cell),
      [self, cell](value_type vals) {
        cell->set_value(std::move(vals));
        self->run_actions();
      },
      [self, cell](error err) {
        cell->set_error(std::move(err));
        self->run_actions();
      });
  }
};

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
    detail::fan_out_response_type_check<Policy, OnValue, OnError, Results...>();
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
    detail::fan_out_response_type_check<Policy, OnValue, OnError, Results...>();
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

  auto as_single() &&
    requires (!std::same_as<type_list<Results...>, type_list<message>>)
  {
    return std::move(*this).template as_single_helper<Results...>();
  }

  // Overload for dynamically typed response.
  template <class... Ts>
  auto as_single() &&
    requires std::same_as<type_list<Results...>, type_list<message>>
  {
    return std::move(*this).template as_single_helper<Ts...>();
  }

  auto as_observable() &&
    requires (!std::same_as<type_list<Results...>, type_list<message>>)
  {
    return std::move(*this).template as_single_helper<Results...>().as_observable();
  }

  // Overload for dynamically typed response.
  template <class... Ts>
  auto as_observable() &&
    requires std::same_as<type_list<Results...>, type_list<message>>
  {
    return std::move(*this).template as_single_helper<Ts...>().as_observable();
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

  template <class... Ts>
  auto as_single_helper() && {
    auto lg = log::core::trace("ids_ = {}", state_.mids);
    using behavior_helper
      = detail::fan_out_response_to_flow_cell_helper<Policy, Ts...>;
    auto [cell, on_value, on_error] = behavior_helper::make_behavior(
      state_.self, state_.self->flow_context());
    behavior bhvr;
    if constexpr (std::same_as<Policy, policy::select_all_tag_t>) {
      bhvr = make_select_all_behavior(std::move(on_value), std::move(on_error));
    } else {
      // For select_any with multiple parameters, create a cow_tuple wrapper.
      if constexpr (sizeof...(Ts) > 1) {
        auto wrapper = [on_value = std::move(on_value)](Ts... args) mutable {
          on_value(cow_tuple<Ts...>{std::move(args)...});
        };
        bhvr = make_select_any_behavior(std::move(wrapper),
                                        std::move(on_error));
      } else {
        bhvr = make_select_any_behavior(std::move(on_value),
                                        std::move(on_error));
      }
    }
    for (const auto& mid : state_.mids)
      state_.self->add_multiplexed_response_handler(mid, bhvr,
                                                    state_.pending_timeout);
    using val_t = typename decltype(cell)::value_type::output_type;
    return flow::single<val_t>{std::move(cell)};
  }
};

/// Similar to `event_based_fan_out_response_handle`, but also holds the
/// `disposable` for the delayed request message.
template <class Policy, class... Results>
class event_based_fan_out_delayed_response_handle {
public:
  using decorated_type
    = event_based_fan_out_response_handle<Policy, Results...>;

  // -- constructors, destructors, and assignment operators --------------------

  event_based_fan_out_delayed_response_handle(abstract_scheduled_actor* self,
                                              std::vector<message_id> mids,
                                              disposable pending_timeout,
                                              disposable pending_request)
    : decorated(self, std::move(mids), std::move(pending_timeout)),
      pending_request(std::move(pending_request)) {
    // nop
  }

  // -- then and await ---------------------------------------------------------

  /// @copydoc event_based_response_handle::await
  template <class OnValue, class OnError>
  disposable await(OnValue on_value, OnError on_error) && {
    std::move(decorated).await(std::move(on_value), std::move(on_error));
    return std::move(pending_request);
  }

  /// @copydoc event_based_response_handle::await
  template <class OnValue>
  disposable await(OnValue on_value) && {
    std::move(decorated).await(std::move(on_value));
    return std::move(pending_request);
  }

  /// @copydoc event_based_response_handle::then
  template <class OnValue, class OnError>
  disposable then(OnValue on_value, OnError on_error) && {
    std::move(decorated).then(std::move(on_value), std::move(on_error));
    return std::move(pending_request);
  }

  /// @copydoc event_based_response_handle::then
  template <class OnValue>
  disposable then(OnValue on_value) && {
    std::move(decorated).then(std::move(on_value));
    return std::move(pending_request);
  }

  auto as_single() &&                                                     //
    requires(not std::same_as<type_list<Results...>, type_list<message>>) //
  { return std::move(decorated).as_single(); }

    // Overload for dynamically typed response.
    template <class... Ts>
    auto as_single() && //
      requires std::same_as<type_list<Results...>, type_list<message>>
  {
    return std::move(decorated).template as_single<Ts...>();
  }

  auto as_observable()
    && requires(
      not std::same_as<
        type_list<Results...>,
        type_list<message>>) { return std::move(decorated).as_observable(); }

    // Overload for dynamically typed response.
    template <class... Ts>
    auto as_observable() &&
      requires std::same_as<type_list<Results...>, type_list<message>>
  {
    return std::move(decorated).template as_observable<Ts...>();
  }

  // -- properties -------------------------------------------------------------

  /// The wrapped handle type.
  decorated_type decorated;

  /// Stores a handle to the in-flight request if the request messages was
  /// delayed/scheduled.
  disposable pending_request;
};

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

// tuple-like access for event_based_fan_out_delayed_response_handle

template <size_t I, class... Results>
decltype(auto)
get(caf::event_based_fan_out_delayed_response_handle<Results...>& x) {
  if constexpr (I == 0) {
    return x.decorated;
  } else {
    static_assert(I == 1);
    return x.pending_request;
  }
}

template <size_t I, class... Results>
decltype(auto)
get(const caf::event_based_fan_out_delayed_response_handle<Results...>& x) {
  if constexpr (I == 0) {
    return x.decorated;
  } else {
    static_assert(I == 1);
    return x.pending_request;
  }
}

template <size_t I, class... Results>
decltype(auto)
get(caf::event_based_fan_out_delayed_response_handle<Results...>&& x) {
  if constexpr (I == 0) {
    return std::move(x.decorated);
  } else {
    static_assert(I == 1);
    return std::move(x.pending_request);
  }
}

} // namespace caf

// enable structured bindings for event_based_fan_out_delayed_response_handle

namespace std {

template <class Policy, class... Results>
struct tuple_size<
  caf::event_based_fan_out_delayed_response_handle<Policy, Results...>> {
  static constexpr size_t value = 2;
};

template <class Policy, class... Results>
struct tuple_element<
  0, caf::event_based_fan_out_delayed_response_handle<Policy, Results...>> {
  using type = caf::event_based_fan_out_response_handle<Results...>;
};

template <class... Results>
struct tuple_element<
  1, caf::event_based_fan_out_delayed_response_handle<Results...>> {
  using type = caf::disposable;
};

} // namespace std
