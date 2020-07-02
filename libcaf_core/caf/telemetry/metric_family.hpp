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

#include <memory>
#include <string>
#include <vector>

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

namespace caf::telemetry {

/// Manages a collection (family) of metrics. All children share the same
/// prefix, name, and label dimensions.
class CAF_CORE_EXPORT metric_family {
public:
  // -- constructors, destructors, and assignment operators --------------------

  metric_family(metric_type type, std::string prefix, std::string name,
                std::vector<std::string> label_names, std::string helptext,
                std::string unit, bool is_sum)
    : type_(type),
      prefix_(std::move(prefix)),
      name_(std::move(name)),
      label_names_(std::move(label_names)),
      helptext_(std::move(helptext)),
      unit_(std::move(unit)),
      is_sum_(is_sum) {
    // nop
  }

  metric_family(const metric_family&) = delete;

  metric_family& operator=(const metric_family&) = delete;

  virtual ~metric_family();

  // -- properties -------------------------------------------------------------

  auto type() const noexcept {
    return type_;
  }

  const auto& prefix() const noexcept {
    return prefix_;
  }

  const auto& name() const noexcept {
    return name_;
  }

  const auto& label_names() const noexcept {
    return label_names_;
  }

  const auto& helptext() const noexcept {
    return helptext_;
  }

  const auto& unit() const noexcept {
    return unit_;
  }

  auto is_sum() const noexcept {
    return is_sum_;
  }

private:
  metric_type type_;
  std::string prefix_;
  std::string name_;
  std::vector<std::string> label_names_;
  std::string helptext_;
  std::string unit_;
  bool is_sum_;
};

} // namespace caf::telemetry
