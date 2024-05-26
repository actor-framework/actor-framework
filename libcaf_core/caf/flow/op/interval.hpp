// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/op/cold.hpp"
#include "caf/timespan.hpp"

#include <cstdint>

namespace caf::flow::op {

class CAF_CORE_EXPORT interval : public cold<int64_t> {
public:
  // -- member types -----------------------------------------------------------

  using super = cold<int64_t>;

  // -- constructors, destructors, and assignment operators --------------------

  interval(coordinator* parent, timespan initial_delay, timespan period);

  interval(coordinator* parent, timespan initial_delay, timespan period,
           int64_t max_val);

  // -- implementation of observable_impl<T> -----------------------------------

  disposable subscribe(observer<int64_t> out) override;

private:
  timespan initial_delay_;
  timespan period_;
  int64_t max_;
};

} // namespace caf::flow::op
