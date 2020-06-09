/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

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

private:
  // -- member variables -------------------------------------------------------

  std::vector<label> labels_;
};

} // namespace caf::telemetry
