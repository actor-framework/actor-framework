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

telemetry::int_gauge*
metric_registry::int_gauge(string_view prefix, string_view name,
                           std::vector<label_view> labels) {
  // Make sure labels are sorted by name.
  auto cmp = [](const label_view& x, const label_view& y) {
    return x.name() < y.name();
  };
  std::sort(labels.begin(), labels.end(), cmp);
  // Fetch the family.
  auto matches = [&](const auto& family) {
    auto lbl_cmp = [](const label_view& lbl, string_view name) {
      return lbl.name() == name;
    };
    return family->prefix() == prefix && family->name() == name
           && std::equal(labels.begin(), labels.end(),
                         family->label_names().begin(),
                         family->label_names().end(), lbl_cmp);
  };
  auto i = std::find_if(int_gauges.begin(), int_gauges.end(), matches);
  if (i == int_gauges.end()) {
    return nullptr;
  }
  return (*i)->get_or_add(labels);
}

namespace {

template <class Type>
using metric_family_ptr = std::unique_ptr<metric_family_impl<Type>>;

template <class T, class... Ts>
void emplace_family(std::vector<metric_family_ptr<T>>& families, Ts&&... xs) {
  using family_type = metric_family_impl<T>;
  families.emplace_back(std::make_unique<family_type>(std::forward<Ts>(xs)...));
}

} // namespace

void metric_registry::add_family(metric_type type, std::string prefix,
                                 std::string name,
                                 std::vector<std::string> label_names,
                                 std::string helptext, std::string unit,
                                 bool is_sum) {
  std::sort(label_names.begin(), label_names.end());
  switch (type) {
    case metric_type::int_gauge:
      return emplace_family(int_gauges, std::move(prefix), std::move(name),
                            std::move(label_names), std::move(helptext),
                            std::move(unit), is_sum);
    default:
      break;
  }
}

} // namespace caf::telemetry
