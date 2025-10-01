// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor_traits.hpp"
#include "caf/behavior.hpp"
#include "caf/cow_tuple.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/flow/fwd.hpp"
#include "caf/fwd.hpp"
#include "caf/local_actor.hpp"
#include "caf/unit.hpp"

namespace caf::detail {

template <class>
struct response_to_flow_helper;

template <>
struct response_to_flow_helper<type_list<>> {
  using value_type = unit_t;

  template <class SinkPtr, class CanEmit, class CanClose>
  static auto
  make_callback(const SinkPtr& sink, CanEmit can_emit, CanClose can_close) {
    using element_type = typename SinkPtr::element_type;
    return [sink, can_emit, can_close] {
      if (can_emit()) {
        if constexpr (element_type::single_value) {
          sink->set_value(unit);
          std::ignore = can_close; // suppress unused warning
        } else {
          sink->push(unit);
          if (can_close()) {
            sink->close();
          }
        }
      }
    };
  };
};

template <class T>
struct response_to_flow_helper<type_list<T>> {
  using value_type = T;

  template <class SinkPtr, class CanEmit, class CanClose>
  static auto
  make_callback(const SinkPtr& sink, CanEmit can_emit, CanClose can_close) {
    using element_type = typename SinkPtr::element_type;
    return [sink, can_emit, can_close](T& value) {
      if (can_emit()) {
        if constexpr (element_type::single_value) {
          sink->set_value(std::move(value));
          std::ignore = can_close; // suppress unused warning
        } else {
          sink->push(value);
          if (can_close()) {
            sink->close();
          }
        }
      }
    };
  };
};

template <class T1, class T2, class... Ts>
struct response_to_flow_helper<type_list<T1, T2, Ts...>> {
  using value_type = cow_tuple<T1, T2, Ts...>;

  template <class SinkPtr, class CanEmit, class CanClose>
  static auto
  make_callback(const SinkPtr& sink, CanEmit can_emit, CanClose can_close) {
    using element_type = typename SinkPtr::element_type;
    return [sink, can_emit, can_close](T1& val1, T2& val2, Ts&... vals) {
      if (can_emit()) {
        auto tup = make_cow_tuple(std::move(val1), std::move(val2),
                                  std::move(vals)...);
        if constexpr (element_type::single_value) {
          sink->set_value(std::move(tup));
          std::ignore = can_close; // suppress unused warning
        } else {
          sink->push(tup);
          if (can_close()) {
            sink->close();
          }
        }
      }
    };
  };
};

template <class... Ts>
struct response_to_flow_helper<cow_tuple<Ts...>>
  : response_to_flow_helper<type_list<Ts...>> {};

} // namespace caf::detail

namespace caf {

/// A cooperatively scheduled, event-based actor implementation.
class CAF_CORE_EXPORT abstract_scheduled_actor
  : public local_actor,
    public non_blocking_actor_base {
public:
  // -- friends ----------------------------------------------------------------

  template <class...>
  friend class event_based_response_handle;

  template <class, class...>
  friend class event_based_fan_out_response_handle;

  // -- member types -----------------------------------------------------------

  using super = local_actor;

  // -- constructors, destructors, and assignment operators --------------------

  using super::super;

  ~abstract_scheduled_actor() override;

  // -- message processing -----------------------------------------------------

  /// Adds a callback for an awaited response.
  virtual void add_awaited_response_handler(message_id response_id,
                                            behavior bhvr,
                                            disposable pending_timeout = {})
    = 0;

  /// Adds a callback for a multiplexed response.
  virtual void add_multiplexed_response_handler(message_id response_id,
                                                behavior bhvr,
                                                disposable pending_timeout = {})
    = 0;

  /// Calls the default error handler.
  virtual void call_error_handler(error& what) = 0;

  /// Runs all pending actions.
  virtual void run_actions() = 0;

  template <class TypeToken, class State>
  auto response_to_single(TypeToken token, const State& state) {
    static_assert(flow::has_impl_include<scheduled_actor, State>,
                  "include 'caf/scheduled_actor/flow.hpp' for this method");
    using helper_type = detail::response_to_flow_helper<TypeToken>;
    using value_type = typename helper_type::value_type;
    return response_to_single_impl<value_type>(token, state);
  }

  template <class TypeToken, class State, class Tag>
  auto response_to_observable(TypeToken token, const State& state, Tag tag) {
    static_assert(flow::has_impl_include<scheduled_actor, State>,
                  "include 'caf/scheduled_actor/flow.hpp' for this method");
    using helper_type = detail::response_to_flow_helper<TypeToken>;
    using value_type = typename helper_type::value_type;
    return response_to_observable_impl<value_type>(token, state, tag);
  }

private:
  template <class T, class TypeToken, class State>
  flow::single<T> response_to_single_impl(TypeToken, const State&);

  template <class T, class TypeToken, class State, class Tag>
  flow::observable<T> response_to_observable_impl(TypeToken, const State&, Tag);

  virtual flow::coordinator* flow_context() = 0;
};

} // namespace caf
