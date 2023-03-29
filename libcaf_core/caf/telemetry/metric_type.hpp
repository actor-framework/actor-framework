// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstdint>

namespace caf::telemetry {

enum class metric_type : uint8_t {
  dbl_counter,
  int_counter,
  dbl_gauge,
  int_gauge,
  dbl_histogram,
  int_histogram,
};

} // namespace caf::telemetry
