// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/flow/observer.hpp"
#include "caf/flow/op/cold.hpp"
#include "caf/flow/subscription.hpp"

#include <utility>

namespace caf::flow::op {

template <class Fn>
struct defer_trait {
  using fn_result_type = decltype(std::declval<Fn&>()());

  static_assert(is_observable_v<fn_result_type>);

  using output_type = typename fn_result_type::output_type;
};

/// Implementation of the `defer` operator.
template <class Factory>
class defer : public cold<typename defer_trait<Factory>::output_type> {
public:
  // -- member types -----------------------------------------------------------

  using output_type = typename defer_trait<Factory>::output_type;

  using super = cold<output_type>;

  // -- constructors, destructors, and assignment operators --------------------

  defer(coordinator* parent, Factory fn) : super(parent), fn_(std::move(fn)) {
    // nop
  }

  // -- implementation of observable<T>::impl ----------------------------------

  disposable subscribe(observer<output_type> what) override {
    return fn_().subscribe(what);
  }

private:
  Factory fn_;
};

} // namespace caf::flow::op
