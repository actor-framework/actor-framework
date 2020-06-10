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
#include "caf/telemetry/metric_family_impl.hpp"

namespace caf::telemetry {

/// Manages a collection of metric families.
class CAF_CORE_EXPORT metric_registry {
public:
  template <class Type>
  using metric_family_ptr = std::unique_ptr<metric_family_impl<Type>>;

  template <class Type>
  using metric_family_container = std::vector<metric_family_ptr<Type>>;

  metric_registry();

  ~metric_registry();

  /// Adds a new metric family to the registry.
  /// @param type The kind of the metric, e.g. gauge or count.
  /// @param prefix The prefix (namespace) this family belongs to. Usually the
  ///               application or protocol name, e.g., `http`. The prefix `caf`
  ///               as well as prefixes starting with an underscore are
  ///               reserved.
  /// @param name The human-readable name of the metric, e.g., `requests`.
  /// @param label_names Names for all label dimensions of the metric.
  /// @param helptext Short explanation of the metric.
  /// @param unit Unit of measurement. Please use base units such as `bytes` or
  ///             `seconds` (prefer lowercase). The pseudo-unit `1` identifies
  ///             dimensionless counts.
  /// @param is_sum Setting this to `true` indicates that this metric adds
  ///               something up to a total, where only the total value is of
  ///               interest. For example, the total number of HTTP requests.
  void add_family(metric_type type, std::string prefix, std::string name,
                  std::vector<std::string> label_names, std::string helptext,
                  std::string unit = "1", bool is_sum = false);

  /// Adds a new metric family to the registry.
  /// @param prefix The prefix (namespace) this family belongs to. Usually the
  ///               application or protocol name, e.g., `http`. The prefix `caf`
  ///               as well as prefixes starting with an underscore are
  ///               reserved.
  /// @param name The human-readable name of the metric, e.g., `requests`.
  /// @param label_names Names for all label dimensions of the metric.
  /// @param helptext Short explanation of the metric.
  /// @param unit Unit of measurement. Please use base units such as `bytes` or
  ///             `seconds` (prefer lowercase). The pseudo-unit `1` identifies
  ///             dimensionless counts.
  /// @param is_sum Setting this to `true` indicates that this metric adds
  ///               something up to a total, where only the total value is of
  ///               interest. For example, the total number of HTTP requests.
  template <class Type>
  metric_family_impl<Type>*
  add_family(std::string prefix, std::string name,
             std::vector<std::string> label_names, std::string helptext,
             std::string unit = "1", bool is_sum = false) {
    using family_type = metric_family_impl<Type>;
    std::sort(label_names.begin(), label_names.end());
    auto ptr = std::make_unique<family_type>(std::move(prefix), std::move(name),
                                             std::move(label_names),
                                             std::move(helptext),
                                             std::move(unit), is_sum);
    std::unique_lock<std::mutex> guard{families_mx_};
    assert_unregistered(ptr->prefix(), ptr->name());
    auto& families = container_by_type<Type>();
    families.emplace_back(std::move(ptr));
    return families.back().get();
  }

  /// Adds a new metric singleton, i.e., a family without label dimensions and
  /// thus exactly one metric instance.
  /// @param prefix The prefix (namespace) this family belongs to. Usually the
  ///               application or protocol name, e.g., `http`. The prefix `caf`
  ///               as well as prefixes starting with an underscore are
  ///               reserved.
  /// @param name The human-readable name of the metric, e.g., `requests`.
  /// @param helptext Short explanation of the metric.
  /// @param unit Unit of measurement. Please use base units such as `bytes` or
  ///             `seconds` (prefer lowercase). The pseudo-unit `1` identifies
  ///             dimensionless counts.
  /// @param is_sum Setting this to `true` indicates that this metric adds
  ///               something up to a total, where only the total value is of
  ///               interest. For example, the total number of HTTP requests.
  template <class Type>
  Type* add_singleton(std::string prefix, std::string name,
                      std::string helptext, std::string unit = "1",
                      bool is_sum = false) {
    auto fptr = add_family<Type>(std::move(prefix), std::move(name), {},
                                 std::move(helptext), std::move(unit), is_sum);
    return fptr->get_or_add({});
  }

  telemetry::int_gauge* int_gauge(string_view prefix, string_view name,
                                  std::vector<label_view> labels);

  template <class Collector>
  void collect(Collector& collector) const {
    std::unique_lock<std::mutex> guard{families_mx_};
    for (auto& ptr : int_gauges_)
      ptr->collect(collector);
  }

private:
  /// @pre `families_mx_` is locked.
  void assert_unregistered(const std::string& prefix, const std::string& name);

  template <class Type>
  metric_family_container<Type>& container_by_type();

  mutable std::mutex families_mx_;

  metric_family_container<telemetry::int_gauge> int_gauges_;
};

template <>
inline metric_registry::metric_family_container<int_gauge>&
metric_registry::container_by_type<int_gauge>() {
  return int_gauges_;
}

} // namespace caf::telemetry
