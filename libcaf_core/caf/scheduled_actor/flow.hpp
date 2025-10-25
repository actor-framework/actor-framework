// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor.hpp"
#include "caf/disposable.hpp"
#include "caf/flow/coordinator.hpp"
#include "caf/flow/observable.hpp"
#include "caf/flow/observable_builder.hpp"
#include "caf/flow/op/auto_connect.hpp"
#include "caf/flow/op/cache.hpp"
#include "caf/flow/op/cell.hpp"
#include "caf/flow/op/publish.hpp"
#include "caf/flow/op/ucast.hpp"
#include "caf/flow/single.hpp"
#include "caf/policy/select_all_tag.hpp"
#include "caf/policy/select_any_tag.hpp"
#include "caf/scheduled_actor.hpp"
#include "caf/stream.hpp"
#include "caf/typed_stream.hpp"

namespace caf::flow {

template <>
struct has_impl_include<scheduled_actor> {
  static constexpr bool value = true;
};

} // namespace caf::flow

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
  }
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
  }
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
  }
};

template <class... Ts>
struct response_to_flow_helper<cow_tuple<Ts...>>
  : response_to_flow_helper<type_list<Ts...>> {};

} // namespace caf::detail

namespace caf {

template <class TypeToken, class State>
  requires flow::assert_has_impl_include<State>
auto abstract_scheduled_actor::response_to_single(TypeToken,
                                                  const State& state) {
  using helper_type = detail::response_to_flow_helper<TypeToken>;
  using value_type = typename helper_type::value_type;
  auto cell = make_counted<flow::op::cell<value_type>>(flow_context());
  if constexpr (State::is_fan_out) {
    // For fan-out responses, we need to add the handler for each message ID.
    // This assumes that the fan-out request uses the select any policy. The
    // first value wins. Further, any error will be ignored unless all other
    // requests already have failed to produce a value.
    auto remaining = std::make_shared<size_t>(state.mids.size());
    auto emit_value = [remaining, cell] {
      --*remaining;
      return cell->pending();
    };
    auto bhvr = behavior{
      helper_type::make_callback(cell, emit_value, std::true_type{}),
      [this, cell, remaining](error& err) {
        if (--*remaining == 0 && cell->pending()) {
          cell->set_error(std::move(err));
        }
      },
    };
    for (auto mid : state.mids) {
      add_multiplexed_response_handler(mid, bhvr, state.pending_timeout);
    }
  } else {
    // There is only a single response. Hence, we can simply call the setter on
    // the cell without any additional logic.
    auto bhvr = behavior{
      helper_type::make_callback(cell, std::true_type{}, std::true_type{}),
      [cell](error& err) { cell->set_error(std::move(err)); },
    };
    add_multiplexed_response_handler(state.mid, bhvr, state.pending_timeout);
  }
  return flow::single<value_type>{std::move(cell)};
}

template <class TypeToken, class State, class Tag>
  requires flow::assert_has_impl_include<State>
auto abstract_scheduled_actor::response_to_observable(TypeToken,
                                                      const State& state, Tag) {
  using helper_type = detail::response_to_flow_helper<TypeToken>;
  using value_type = typename helper_type::value_type;
  if constexpr (State::is_fan_out) {
    auto sink = make_counted<flow::op::ucast<value_type>>(flow_context());
    behavior bhvr;
    if constexpr (std::same_as<Tag, policy::select_all_tag_t>) {
      struct callback_state_t {
        size_t remaining;
        bool failed;
      };
      auto callback_state = std::make_shared<callback_state_t>(callback_state_t{
        .remaining = state.mids.size(),
        .failed = false,
      });
      auto can_emit = [callback_state] {
        auto& [remaining, failed] = *callback_state;
        --remaining;
        return !failed;
      };
      auto can_close = [callback_state] {
        return callback_state->remaining == 0;
      };
      bhvr = behavior{
        helper_type::make_callback(sink, can_emit, can_close),
        [sink, callback_state](error& err) {
          if (auto& failed = callback_state->failed; !failed) {
            failed = true;
            sink->abort(err);
          }
        },
      };
    } else {
      static_assert(std::same_as<Tag, policy::select_any_tag_t>);
      struct callback_state_t {
        size_t remaining;
        bool pending;
      };
      auto callback_state = std::make_shared<callback_state_t>(callback_state_t{
        .remaining = state.mids.size(),
        .pending = true,
      });
      auto can_emit = [callback_state] {
        auto& [remaining, pending] = *callback_state;
        --remaining;
        if (pending) {
          pending = false;
          return true;
        }
        return false;
      };
      bhvr = behavior{
        helper_type::make_callback(sink, can_emit, std::true_type{}),
        [sink, callback_state](error& err) {
          auto& [remaining, pending] = *callback_state;
          if (--remaining == 0 && pending) {
            sink->abort(err);
          }
        },
      };
    }
    for (auto mid : state.mids) {
      add_multiplexed_response_handler(mid, bhvr, state.pending_timeout);
    }
    // Subscribe a cache to the sink, turning into a cold observable that can be
    // observed at any time without losing any data.
    auto cache = make_counted<flow::op::cache<value_type>>(flow_context(),
                                                           std::move(sink));
    cache->subscribe_to_source();
    return flow::observable<value_type>{std::move(cache)};
  } else {
    // There is only a single response. Hence, we can simply use a cell, like we
    // do for in response_to_single_impl.
    auto cell = make_counted<flow::op::cell<value_type>>(flow_context());
    auto bhvr = behavior{
      helper_type::make_callback(cell, std::true_type{}, std::true_type{}),
      [cell](error& err) { cell->set_error(std::move(err)); },
    };
    add_multiplexed_response_handler(state.mid, bhvr, state.pending_timeout);
    return flow::observable<value_type>{std::move(cell)};
  }
}

template <class T>
  requires flow::assert_has_impl_include<T>
auto scheduled_actor::observe(typed_stream<T> what, size_t buf_capacity,
                              size_t demand_threshold) {
  return do_observe(what.dynamically_typed(), buf_capacity, demand_threshold)
    .transform(detail::unbatch<T>{})
    .as_observable();
}

template <class T>
  requires flow::assert_has_impl_include<T>
auto scheduled_actor::observe_as(stream what, size_t buf_capacity,
                                 size_t demand_threshold) {
  if (what.template has_element_type<T>())
    return do_observe(what, buf_capacity, demand_threshold)
      .transform(detail::unbatch<T>{})
      .as_observable();
  return make_observable().fail<T>(make_error(sec::type_clash));
}

} // namespace caf
