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
  using family_type = metric_family_impl<telemetry::int_gauge>;
  family_type* fptr;
  {
    std::unique_lock<std::mutex> guard{families_mx_};
    auto i = std::find_if(int_gauges_.begin(), int_gauges_.end(), matches);
    if (i == int_gauges_.end()) {
      std::string prefix_str{prefix.begin(), prefix.end()};
      std::string name_str{name.begin(), name.end()};
      assert_unregistered(prefix_str, name_str);
      std::vector<std::string> label_names;
      label_names.reserve(labels.size());
      for (auto& lbl : labels) {
        auto lbl_name = lbl.name();
        label_names.emplace_back(std::string{lbl_name.begin(), lbl_name.end()});
      }
      auto ptr = std::make_unique<family_type>(std::move(prefix_str),
                                               std::move(name_str),
                                               std::move(label_names), "", "1",
                                               false);
      i = int_gauges_.emplace(i, std::move(ptr));
    }
    fptr = i->get();
  }
  return fptr->get_or_add(labels);
}

void metric_registry::add_family(metric_type type, std::string prefix,
                                 std::string name,
                                 std::vector<std::string> label_names,
                                 std::string helptext, std::string unit,
                                 bool is_sum) {
  namespace t = caf::telemetry;
  switch (type) {
    case metric_type::int_gauge:
      add_family<t::int_gauge>(std::move(prefix), std::move(name),
                               std::move(label_names), std::move(helptext),
                               std::move(unit), is_sum);
      break;
    default:
      CAF_RAISE_ERROR("invalid metric type");
  }
}

void metric_registry::assert_unregistered(const std::string& prefix,
                                          const std::string& name) {
  auto matches = [&](const auto& ptr) {
    return ptr->prefix() == prefix && ptr->name() == name;
  };
  if (std::any_of(int_gauges_.begin(), int_gauges_.end(), matches))
    CAF_RAISE_ERROR("prefix and name already belong to an int gauge family");
}

} // namespace caf::telemetry
