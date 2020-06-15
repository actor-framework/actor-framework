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

#include "caf/telemetry/metric_registry.hpp"

#include "caf/config.hpp"
#include "caf/raise_error.hpp"
#include "caf/telemetry/dbl_gauge.hpp"
#include "caf/telemetry/int_gauge.hpp"
#include "caf/telemetry/metric_family_impl.hpp"
#include "caf/telemetry/metric_impl.hpp"
#include "caf/telemetry/metric_type.hpp"

namespace caf::telemetry {

metric_registry::metric_registry() {
  // nop
}

metric_registry::~metric_registry() {
  // nop
}

metric_family* metric_registry::fetch(const string_view& prefix,
                                      const string_view& name) {
  auto matches = [&](const auto& ptr) {
    return ptr->prefix() == prefix && ptr->name() == name;
  };
  if (auto i = std::find_if(dbl_gauges_.begin(), dbl_gauges_.end(), matches);
      i != dbl_gauges_.end())
    return i->get();
  if (auto i = std::find_if(int_gauges_.begin(), int_gauges_.end(), matches);
      i != int_gauges_.end())
    return i->get();
  return nullptr;
}

void metric_registry::assert_properties(metric_family* ptr, metric_type type,
                                        span<const string_view> label_names,
                                        string_view unit, bool is_sum) {
  auto labels_match = [&] {
    const auto& xs = ptr->label_names();
    const auto& ys = label_names;
    return std::is_sorted(ys.begin(), ys.end())
             ? std::equal(xs.begin(), xs.end(), ys.begin(), ys.end())
             : std::is_permutation(xs.begin(), xs.end(), ys.begin(), ys.end());
  };
  if (ptr->type() != type)
    CAF_RAISE_ERROR("full name with different metric type found");
  if (!labels_match())
    CAF_RAISE_ERROR("full name with different label dimensions found");
  if (ptr->unit() != unit)
    CAF_RAISE_ERROR("full name with different unit found");
  if (ptr->is_sum() != is_sum)
    CAF_RAISE_ERROR("full name with different is-sum flag found");
}

} // namespace caf::telemetry
