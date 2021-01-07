// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <string>
#include <utility>
#include <vector>

#include "caf/fwd.hpp"
#include "caf/string_view.hpp"
#include "caf/telemetry/label.hpp"

namespace caf::telemetry {

/// A single metric, identified by the values it sets for the label dimensions.
class CAF_CORE_EXPORT metric {
public:
  // -- constructors, destructors, and assignment operators --------------------

  metric(std::vector<label> labels) : labels_(std::move(labels)) {
    // nop
  }

  virtual ~metric();

  // -- properties -------------------------------------------------------------

  const auto& labels() const noexcept {
    return labels_;
  }

protected:
  // -- member variables -------------------------------------------------------

  std::vector<label> labels_;
};

} // namespace caf::telemetry
