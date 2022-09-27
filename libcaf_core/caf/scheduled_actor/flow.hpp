// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

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

namespace caf::flow {

template <>
struct has_impl_include<scheduled_actor> {
  static constexpr bool value = true;
};

} // namespace caf::flow

namespace caf::detail {

template <class T>
class unbatch {
public:
  using input_type = async::batch;

  using output_type = T;

  template <class Next, class... Steps>
  bool on_next(const async::batch& xs, Next& next, Steps&... steps) {
    for (const auto& item : xs.template items<T>())
      if (!next.on_next(item, steps...))
        return false;
    return true;
  }

  template <class Next, class... Steps>
  void on_complete(Next& next, Steps&... steps) {
    next.on_complete(steps...);
  }

  template <class Next, class... Steps>
  void on_error(const error& what, Next& next, Steps&... steps) {
    next.on_error(what, steps...);
  }
};

template <class T>
struct batching_trait {
  static constexpr bool skip_empty = true;
  using input_type = T;
  using output_type = async::batch;
  using select_token_type = int64_t;

  output_type operator()(const std::vector<input_type>& xs) {
    return async::make_batch(make_span(xs));
  }
};

} // namespace caf::detail

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

template <class Observable>
flow::assert_scheduled_actor_hdr_t<Observable, stream>
scheduled_actor::to_stream(std::string name, timespan max_delay,
                           size_t max_items_per_batch, Observable&& obs) {
  return to_stream(cow_string{std::move(name)}, max_delay, max_items_per_batch,
                   std::forward<Observable>(obs));
}

template <class Observable>
flow::assert_scheduled_actor_hdr_t<Observable, stream>
scheduled_actor::to_stream(cow_string name, timespan max_delay,
                           size_t max_items_per_batch, Observable&& obs) {
  using obs_t = std::decay_t<Observable>;
  using val_t = typename obs_t::output_type;
  using trait_t = detail::batching_trait<val_t>;
  using impl_t = flow::op::buffer<trait_t>;
  auto batch_op = make_counted<impl_t>(
    this, max_items_per_batch, std::forward<Observable>(obs).as_observable(),
    flow::make_observable<flow::op::interval>(this, max_delay, max_delay));
  return to_stream_impl(std::move(name), std::move(batch_op), type_id_v<val_t>,
                        max_items_per_batch);
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

} // namespace caf
