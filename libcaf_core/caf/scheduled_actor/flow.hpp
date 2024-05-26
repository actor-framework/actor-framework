// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor.hpp"
#include "caf/disposable.hpp"
#include "caf/flow/coordinator.hpp"
#include "caf/flow/observable.hpp"
#include "caf/flow/observable_builder.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/op/buffer.hpp"
#include "caf/flow/op/cell.hpp"
#include "caf/flow/single.hpp"
#include "caf/scheduled_actor.hpp"
#include "caf/stream.hpp"
#include "caf/typed_stream.hpp"

namespace caf::flow {

template <>
struct has_impl_include<scheduled_actor> {
  static constexpr bool value = true;
};

} // namespace caf::flow

namespace caf {

template <class T, class Policy>
flow::single<T> scheduled_actor::single_from_response_impl(Policy& policy) {
  auto cell = make_counted<flow::op::cell<T>>(this);
  policy.then(
    this,
    [this, cell](T& val) {
      cell->set_value(std::move(val));
      run_actions();
    },
    [this, cell](error& err) {
      cell->set_error(std::move(err));
      run_actions();
    });
  return flow::single<T>{std::move(cell)};
}

template <class T>
flow::assert_scheduled_actor_hdr_t<flow::observable<T>>
scheduled_actor::observe(typed_stream<T> what, size_t buf_capacity,
                         size_t demand_threshold) {
  return do_observe(what.dynamically_typed(), buf_capacity, demand_threshold)
    .transform(detail::unbatch<T>{})
    .as_observable();
}

template <class T>
flow::assert_scheduled_actor_hdr_t<flow::observable<T>>
scheduled_actor::observe_as(stream what, size_t buf_capacity,
                            size_t demand_threshold) {
  if (what.template has_element_type<T>())
    return do_observe(what, buf_capacity, demand_threshold)
      .transform(detail::unbatch<T>{})
      .as_observable();
  return make_observable().fail<T>(make_error(sec::type_clash));
}

template <class T>
flow::assert_scheduled_actor_hdr_t<flow::single<T>>
scheduled_actor::single_from_response(message_id mid,
                                      disposable pending_timeout) {
  auto cell = make_counted<flow::op::cell<T>>(this);
  auto bhvr = behavior{
    [this, cell](T& val) {
      cell->set_value(std::move(val));
      run_actions();
    },
    [this, cell](error& err) {
      cell->set_error(std::move(err));
      run_actions();
    },
  };
  add_multiplexed_response_handler(mid, std::move(bhvr),
                                   std::move(pending_timeout));
  return flow::single<T>{std::move(cell)};
}

} // namespace caf
