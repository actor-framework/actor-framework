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

#include <map>
#include <memory>
#include <mutex>

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/string_view.hpp"

namespace caf::telemetry {

/// Manages a collection of metric families.
class CAF_CORE_EXPORT metric_registry {
public:
  metric_registry();

  ~metric_registry();

  void add_family(metric_type type, std::string prefix, std::string name,
                  std::vector<std::string> label_names, std::string helptext);

  telemetry::int_gauge* int_gauge(string_view prefix, string_view name,
                                  std::vector<label_view> labels);

private:
  void add_int_gauge_family(std::string prefix, std::string name,
                            std::vector<std::string> label_names,
                            std::string helptext);

  template <class Type>
  using metric_family_ptr = std::unique_ptr<metric_family_impl<Type>>;

  std::vector<metric_family_ptr<telemetry::int_gauge>> int_gauges;
};

} // namespace caf::telemetry
