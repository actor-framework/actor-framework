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
#include "caf/policy/select_any.hpp"
#include "caf/type_list.hpp"

#include <type_traits>

namespace caf::policy {
struct select_all_tag_t {};
constexpr auto select_all_v = select_all_tag_t{};

struct select_any_tag_t {};
constexpr auto select_any_v = select_any_tag_t{};
} // namespace caf::policy

namespace caf::detail1 {

template <class... Ts>
struct select_all_helper_value_oracle {
  using type = std::tuple<Ts...>;
};

template <class T>
struct select_all_helper_value_oracle<T> {
  using type = T;
};

template <class... Ts>
using select_all_helper_value_t =
  typename select_all_helper_value_oracle<Ts...>::type;

template <class F, class... Ts>
struct select_all_helper;

template <class F, class T, class... Ts>
struct select_all_helper<F, T, Ts...> {
  using value_type = select_all_helper_value_t<T, Ts...>;
  // using value_type
  //   = std::conditional_t<sizeof...(Ts) == 0, T, std::tuple<T, Ts...>>;
  std::vector<value_type> results;
  std::shared_ptr<size_t> pending;
  disposable timeouts;
  F f;

  template <class Fun>
  select_all_helper(size_t pending, disposable timeouts, Fun&& f)
    : pending(std::make_shared<size_t>(pending)),
      timeouts(std::move(timeouts)),
      f(std::forward<Fun>(f)) {
    results.reserve(pending);
  }

  void operator()(T& x, Ts&... xs) {
    auto lg = log::core::trace("pending = {}", *pending);
    if (*pending > 0) {
      results.emplace_back(std::move(x), std::move(xs)...);
      if (--*pending == 0) {
        timeouts.dispose();
        f(std::move(results));
      }
    }
  }

  auto wrap() {
    return [this](T& x, Ts&... xs) { (*this)(x, xs...); };
  }
};

template <class F>
struct select_all_helper<F> {
  std::shared_ptr<size_t> pending;
  disposable timeouts;
  F f;

  template <class Fun>
  select_all_helper(size_t pending, disposable timeouts, Fun&& f)
    : pending(std::make_shared<size_t>(pending)),
      timeouts(std::move(timeouts)),
      f(std::forward<Fun>(f)) {
    // nop
  }

  void operator()() {
    auto lg = log::core::trace("pending = {}", *pending);
    if (*pending > 0 && --*pending == 0) {
      timeouts.dispose();
      f();
    }
  }

  auto wrap() {
    return [this] { (*this)(); };
  }
};

template <class F, class = typename detail::get_callable_trait<F>::arg_types>
struct select_select_all_helper;

template <class F, class... Ts>
struct select_select_all_helper<F, type_list<std::vector<std::tuple<Ts...>>>> {
  using type = select_all_helper<F, Ts...>;
};

template <class F, class T>
struct select_select_all_helper<F, type_list<std::vector<T>>> {
  using type = select_all_helper<F, T>;
};

template <class F>
struct select_select_all_helper<F, type_list<>> {
  using type = select_all_helper<F>;
};

template <class F>
using select_all_helper_t = typename select_select_all_helper<F>::type;

template <class F, class = typename detail::get_callable_trait<F>::arg_types>
struct select_any_factory;

template <class F, class... Ts>
struct select_any_factory<F, type_list<Ts...>> {
  template <class Fun>
  static auto
  make(std::shared_ptr<size_t> pending, disposable timeouts, Fun f) {
    return [pending{std::move(pending)}, timeouts{std::move(timeouts)},
            f{std::move(f)}](Ts... xs) mutable {
      auto lg = log::core::trace("pending = {}", *pending);
      if (*pending > 0) {
        timeouts.dispose();
        f(xs...);
        *pending = 0;
      }
    };
  }
};

} // namespace caf::detail1

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

// template <class Result>
// struct event_based_delayed_response_handle_oracle;
//
// template <>
// struct event_based_delayed_response_handle_oracle<message> {
//   using type = event_based_delayed_response_handle<message>;
// };
//
// template <>
// struct event_based_delayed_response_handle_oracle<type_list<void>> {
//   using type = event_based_delayed_response_handle<>;
// };
//
// template <class... Results>
// struct event_based_delayed_response_handle_oracle<type_list<Results...>> {
//   using type = event_based_delayed_response_handle<Results...>;
// };
//
// template <class Result>
// using event_based_delayed_response_handle_t =
//   typename event_based_delayed_response_handle_oracle<Result>::type;

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
    if constexpr (std::is_same_v<Policy, policy::select_all_tag_t>) {
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
    if constexpr (std::is_same_v<Policy, policy::select_all_tag_t>) {
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

  template <class T>
  auto as_single() && {
    auto lg = log::core::trace("ids_ = {}", state_.mids);
    if constexpr (std::is_same_v<Policy, policy::select_all_tag_t>) {
      auto cell = make_counted<flow::op::cell<std::vector<T>>>(
        state_.self->flow_context());
      auto bhvr = make_select_all_behavior(
        [cell, self = state_.self](std::vector<T> value) {
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
      auto cell = make_counted<flow::op::cell<T>>(state_.self->flow_context());
      auto bhvr = make_select_any_behavior(
        [cell, self = state_.self](T value) {
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

/*
/// This helper class identifies an expected response message and enables
/// `request(...).then(...)`.
template <>
class event_based_response_handle<message> {
public:
  // -- friends ----------------------------------------------------------------

  friend class scheduled_actor;

  // -- constructors, destructors, and assignment operators --------------------

  event_based_response_handle(abstract_scheduled_actor* self, message_id mid,
                              disposable pending_timeout)
    : state_{self, mid, std::move(pending_timeout)} {
    // nop
  }

  // -- then and await ---------------------------------------------------------

  template <class OnValue, class OnError>
  void await(OnValue on_value, OnError on_error) && {
    detail::response_type_check<OnValue, OnError, message>();
    auto bhvr = behavior{std::move(on_value), std::move(on_error)};
    state_.self->add_awaited_response_handler(
      state_.mid, std::move(bhvr), std::move(state_.pending_timeout));
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
    detail::response_type_check<OnValue, OnError, message>();
    auto bhvr = behavior{std::move(on_value), std::move(on_error)};
    state_.self->add_multiplexed_response_handler(
      state_.mid, std::move(bhvr), std::move(state_.pending_timeout));
  }

  template <class OnValue>
  void then(OnValue on_value) && {
    return std::move(*this).then(std::move(on_value),
                                 [self = state_.self](error& err) {
                                   self->call_error_handler(err);
                                 });
  }

  template <class... Ts>
  auto as_observable() && {
    auto cell = state_.self->template response_to_flow_cell<Ts...>(
      state_.mid, std::move(state_.pending_timeout));
    using cell_t = typename decltype(cell)::value_type;
    using val_t = typename cell_t::output_type;
    return flow::single<val_t>{std::move(cell)}.as_observable();
  }

private:
  /// Holds the state for the handle.
  event_based_response_handle_state state_;
};

/// Similar to `event_based_response_handle`, but also holds the `disposable`
/// for the delayed request message.
template <class... Results>
class event_based_delayed_response_handle {
public:
  using decorated_type = event_based_response_handle<Results...>;

  // -- constructors, destructors, and assignment operators --------------------

  event_based_delayed_response_handle(abstract_scheduled_actor* self,
                                      message_id mid,
                                      disposable pending_timeout,
                                      disposable pending_request)
    : decorated(self, mid, std::move(pending_timeout)),
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

  auto as_observable() && {
    return std::move(decorated).as_observable();
  }

  // -- properties -------------------------------------------------------------

  /// The wrapped handle type.
  decorated_type decorated;

  /// Stores a handle to the in-flight request if the request messages was
  /// delayed/scheduled.
  disposable pending_request;
};

// Specialization for dynamically typed messages.
template <>
class event_based_delayed_response_handle<message> {
public:
  using decorated_type = event_based_response_handle<message>;

  // -- constructors, destructors, and assignment operators --------------------

  event_based_delayed_response_handle(abstract_scheduled_actor* self,
                                      message_id mid,
                                      disposable pending_timeout,
                                      disposable pending_request)
    : decorated(self, mid, std::move(pending_timeout)),
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

  template <class... Ts>
  auto as_observable() && {
    return std::move(decorated).template as_observable<Ts...>();
  }

  // -- properties -------------------------------------------------------------

  /// The wrapped handle type.
  decorated_type decorated;

  /// Stores a handle to the in-flight request if the request messages was
  /// delayed/scheduled.
  disposable pending_request;
};

// tuple-like access for event_based_delayed_response_handle

template <size_t I, class... Results>
decltype(auto) get(caf::event_based_delayed_response_handle<Results...>& x) {
  if constexpr (I == 0) {
    return x.decorated;
  } else {
    static_assert(I == 1);
    return x.pending_request;
  }
}

template <size_t I, class... Results>
decltype(auto)
get(const caf::event_based_delayed_response_handle<Results...>& x) {
  if constexpr (I == 0) {
    return x.decorated;
  } else {
    static_assert(I == 1);
    return x.pending_request;
  }
}

template <size_t I, class... Results>
decltype(auto) get(caf::event_based_delayed_response_handle<Results...>&& x) {
  if constexpr (I == 0) {
    return std::move(x.decorated);
  } else {
    static_assert(I == 1);
    return std::move(x.pending_request);
  }
}
*/

} // namespace caf

// enable structured bindings for event_based_delayed_response_handle

/*
namespace std {

template <class... Results>
struct tuple_size<caf::event_based_delayed_response_handle<Results...>> {
  static constexpr size_t value = 2;
};

template <class... Results>
struct tuple_element<0, caf::event_based_delayed_response_handle<Results...>> {
  using type = caf::event_based_response_handle<Results...>;
};

template <class... Results>
struct tuple_element<1, caf::event_based_delayed_response_handle<Results...>> {
  using type = caf::disposable;
};

} // namespace std
*/
