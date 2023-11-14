// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/flow/op/pullable.hpp"

#include "caf/flow/coordinator.hpp"

namespace caf::flow::op {

pullable::pullable() {
  pull_cb_ = make_action([this] {
    do_ref();
    do {
      auto val = in_flight_demand_;
      do_pull(val);
      in_flight_demand_ -= val;
    } while (in_flight_demand_ > 0);
    do_deref();
  });
}

pullable::~pullable() {
  pull_cb_.dispose();
}

void pullable::pull(flow::coordinator* parent, size_t n) {
  CAF_ASSERT(n > 0);
  if (in_flight_demand_ == 0)
    parent->delay(pull_cb_);
  in_flight_demand_ += n;
}

} // namespace caf::flow::op
