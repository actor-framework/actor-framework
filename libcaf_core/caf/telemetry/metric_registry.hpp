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

#include <initializer_list>
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
  // -- member types -----------------------------------------------------------

  template <class Type>
  using metric_family_ptr = std::unique_ptr<metric_family_impl<Type>>;

  template <class Type>
  using metric_family_container = std::vector<metric_family_ptr<Type>>;

  // -- constructors, destructors, and assignment operators --------------------

  metric_registry();

  ~metric_registry();

  // -- factories --------------------------------------------------------------

  /// Returns a metric family. Creates the family lazily if necessary, but fails
  /// if the full name already belongs to a different family.
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
  metric_family_impl<Type>* family(string_view prefix, string_view name,
                                   span<const string_view> label_names,
                                   string_view helptext, string_view unit = "1",
                                   bool is_sum = false) {
    using family_type = metric_family_impl<Type>;
    std::unique_lock<std::mutex> guard{families_mx_};
    if (auto ptr = fetch(prefix, name)) {
      assert_properties(ptr, Type::runtime_type, label_names, unit, is_sum);
      return static_cast<family_type*>(ptr);
    }
    auto ptr = std::make_unique<family_type>(to_string(prefix), to_string(name),
                                             to_sorted_vec(label_names),
                                             to_string(helptext),
                                             to_string(unit), is_sum);
    auto& families = container_by_type<Type>();
    families.emplace_back(std::move(ptr));
    return families.back().get();
  }

  /// @copydoc family
  template <class Type>
  metric_family_impl<Type>*
  family(string_view prefix, string_view name,
         std::initializer_list<string_view> label_names, string_view helptext,
         string_view unit = "1", bool is_sum = false) {
    return family<Type>(prefix, name,
                        span<const string_view>{label_names.begin(),
                                                label_names.size()},
                        helptext, unit, is_sum);
  }

  /// Returns a metric singleton, i.e., the single instance of a family without
  /// label dimensions. Creates all objects lazily if necessary, but fails if
  /// the full name already belongs to a different family.
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
  Type* singleton(string_view prefix, string_view name, string_view helptext,
                  string_view unit = "1", bool is_sum = false) {
    auto fptr = family<Type>(prefix, name, {}, helptext, unit, is_sum);
    return fptr->get_or_add({});
  }

  // -- observers --------------------------------------------------------------

  template <class Collector>
  void collect(Collector& collector) const {
    std::unique_lock<std::mutex> guard{families_mx_};
    for (auto& ptr : dbl_gauges_)
      ptr->collect(collector);
    for (auto& ptr : int_gauges_)
      ptr->collect(collector);
  }

private:
  /// @pre `families_mx_` is locked.
  metric_family* fetch(const string_view& prefix, const string_view& name);

  static std::vector<std::string> to_sorted_vec(span<const string_view> xs) {
    std::vector<std::string> result;
    if (!xs.empty()) {
      result.reserve(xs.size());
      for (auto x : xs)
        result.emplace_back(to_string(x));
      std::sort(result.begin(), result.end());
    }
    return result;
  }

  void assert_properties(metric_family* ptr, metric_type type,
                         span<const string_view> label_names, string_view unit,
                         bool is_sum);

  void assert_equal(metric_family* old_ptr, metric_family* new_ptr);

  template <class Type>
  metric_family_container<Type>& container_by_type();

  mutable std::mutex families_mx_;

  metric_family_container<telemetry::dbl_gauge> dbl_gauges_;
  metric_family_container<telemetry::int_gauge> int_gauges_;
};

template <>
inline metric_registry::metric_family_container<dbl_gauge>&
metric_registry::container_by_type<dbl_gauge>() {
  return dbl_gauges_;
}

template <>
inline metric_registry::metric_family_container<int_gauge>&
metric_registry::container_by_type<int_gauge>() {
  return int_gauges_;
}

} // namespace caf::telemetry
