// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/flow/observable.hpp"
#include "caf/flow/observer.hpp"

namespace caf::flow {

/// Combines the items emitted from `pub` and `pubs...` to appear as a single
/// stream of items.
template <class Observable, class... Observables>
observable<typename Observable::output_type>
merge(Observable x, Observables... xs) {
  using output_type = output_type_t<Observable>;
  static_assert(
    (std::is_same_v<output_type, output_type_t<Observables>> && ...));
  auto hdl = std::move(x).as_observable();
  auto ptr = make_counted<merger_impl<output_type>>(hdl.ptr()->ctx());
  ptr->add(std::move(hdl));
  (ptr->add(std::move(xs).as_observable()), ...);
  return observable<output_type>{std::move(ptr)};
}

} // namespace caf::flow
