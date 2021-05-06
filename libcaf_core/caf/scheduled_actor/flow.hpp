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
#include "caf/flow/single.hpp"
#include "caf/scheduled_actor.hpp"

namespace caf::flow {

template <>
struct has_impl_include<scheduled_actor> {
  static constexpr bool value = true;
};

} // namespace caf::flow

namespace caf {

template <class T, class Policy>
flow::single<T> scheduled_actor::single_from_response_impl(Policy& policy) {
  using output_type = T;
  using impl_type = typename flow::single<output_type>::impl;
  auto ptr = make_counted<impl_type>(this);
  policy.then(
    this,
    [this, ptr](T& val) {
      ptr->set_value(std::move(val));
      run_actions();
    },
    [this, ptr](error& err) {
      ptr->set_error(std::move(err));
      run_actions();
    });
  return flow::single<output_type>{std::move(ptr)};
}

} // namespace caf
