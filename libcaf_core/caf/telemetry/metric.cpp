// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/telemetry/metric.hpp"

#include "caf/config.hpp"
#include "caf/telemetry/metric_family.hpp"

namespace caf::telemetry {

metric::~metric() {
  // nop
}

} // namespace caf::telemetry
