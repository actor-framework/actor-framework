// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/assert.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/raise_error.hpp"
#include "caf/settings.hpp"
#include "caf/telemetry/counter.hpp"
#include "caf/telemetry/gauge.hpp"
#include "caf/telemetry/histogram.hpp"
#include "caf/telemetry/metric_family_impl.hpp"

#include <algorithm>
#include <initializer_list>
#include <map>
#include <memory>
#include <mutex>
#include <string_view>

namespace caf::telemetry {

/// Manages a collection of metric families.
class CAF_CORE_EXPORT metric_registry {
public:
  // -- member types -----------------------------------------------------------

  /// Forces the compiler to use the type `span<const T>` instead of trying to
  /// match parameters to a `span`.
  template <class T>
  struct span_type {
    using type = span<const T>;
  };

  /// Convenience alias to safe some typing.
  template <class T>
  using span_t = typename span_type<T>::type;

  // -- constructors, destructors, and assignment operators --------------------

  metric_registry();

  explicit metric_registry(const actor_system_config& cfg);

  ~metric_registry();

  // -- factories --------------------------------------------------------------

  /// Returns a gauge metric family. Creates the family lazily if necessary, but
  /// fails if the full name already belongs to a different family.
  /// @param prefix The prefix (namespace) this family belongs to. Usually the
  ///               application or protocol name, e.g., `http`. The prefix `caf`
  ///               as well as prefixes starting with an underscore are
  ///               reserved.
  /// @param name The human-readable name of the metric, e.g., `requests`.
  /// @param labels Names for all label dimensions of the metric.
  /// @param helptext Short explanation of the metric.
  /// @param unit Unit of measurement. Please use base units such as `bytes` or
  ///             `seconds` (prefer lowercase). The pseudo-unit `1` identifies
  ///             dimensionless counts.
  /// @param is_sum Setting this to `true` indicates that this metric adds
  ///               something up to a total, where only the total value is of
  ///               interest. For example, the total number of HTTP requests.
  template <class ValueType = int64_t>
  metric_family_impl<gauge<ValueType>>*
  gauge_family(std::string_view prefix, std::string_view name,
               span_t<std::string_view> labels, std::string_view helptext,
               std::string_view unit = "1", bool is_sum = false) {
    using gauge_type = gauge<ValueType>;
    using family_type = metric_family_impl<gauge_type>;
    std::unique_lock<std::mutex> guard{families_mx_};
    if (auto ptr = fetch(prefix, name)) {
      assert_properties(ptr, gauge_type::runtime_type, labels, unit, is_sum);
      return static_cast<family_type*>(ptr);
    }
    auto ptr = std::make_unique<family_type>(
      std::string{prefix}, std::string{name}, to_sorted_vec(labels),
      std::string{helptext}, std::string{unit}, is_sum);
    auto result = ptr.get();
    families_.emplace_back(std::move(ptr));
    return result;
  }

  /// @copydoc gauge_family
  template <class ValueType = int64_t>
  metric_family_impl<gauge<ValueType>>*
  gauge_family(std::string_view prefix, std::string_view name,
               std::initializer_list<std::string_view> labels,
               std::string_view helptext, std::string_view unit = "1",
               bool is_sum = false) {
    auto lbl_span = make_span(labels.begin(), labels.size());
    return gauge_family<ValueType>(prefix, name, lbl_span, helptext, unit,
                                   is_sum);
  }

  /// @copydoc gauge_family
  template <class ValueType = int64_t>
  metric_family_impl<gauge<ValueType>>*
  gauge_family(std::string_view prefix, std::string_view name,
               span_t<label_view> labels, std::string_view helptext,
               std::string_view unit = "1", bool is_sum = false) {
    using gauge_type = gauge<ValueType>;
    using family_type = metric_family_impl<gauge_type>;
    std::unique_lock<std::mutex> guard{families_mx_};
    if (auto ptr = fetch(prefix, name)) {
      assert_properties(ptr, gauge_type::runtime_type, labels, unit, is_sum);
      return static_cast<family_type*>(ptr);
    }
    auto ptr = std::make_unique<family_type>(
      std::string{prefix}, std::string{name}, to_sorted_vec(labels),
      std::string{helptext}, std::string{unit}, is_sum);
    auto result = ptr.get();
    families_.emplace_back(std::move(ptr));
    return result;
  }

  /// Returns a gauge. Creates all objects lazily if necessary, but fails if
  /// the full name already belongs to a different family.
  /// @param prefix The prefix (namespace) this family belongs to. Usually the
  ///               application or protocol name, e.g., `http`. The prefix `caf`
  ///               as well as prefixes starting with an underscore are
  ///               reserved.
  /// @param name The human-readable name of the metric, e.g., `requests`.
  /// @param labels Values for all label dimensions of the metric.
  /// @param helptext Short explanation of the metric.
  /// @param unit Unit of measurement. Please use base units such as `bytes` or
  ///             `seconds` (prefer lowercase). The pseudo-unit `1` identifies
  ///             dimensionless counts.
  /// @param is_sum Setting this to `true` indicates that this metric adds
  ///               something up to a total, where only the total value is of
  ///               interest. For example, the total number of HTTP requests.
  template <class ValueType = int64_t>
  gauge<ValueType>*
  gauge_instance(std::string_view prefix, std::string_view name,
                 span_t<label_view> labels, std::string_view helptext,
                 std::string_view unit = "1", bool is_sum = false) {
    auto label_names = get_label_names(labels);
    auto fptr = gauge_family<ValueType>(prefix, name, label_names, helptext,
                                        unit, is_sum);
    return fptr->get_or_add(labels);
  }

  /// @copydoc gauge_instance
  template <class ValueType = int64_t>
  gauge<ValueType>*
  gauge_instance(std::string_view prefix, std::string_view name,
                 std::initializer_list<label_view> labels,
                 std::string_view helptext, std::string_view unit = "1",
                 bool is_sum = false) {
    span_t<label_view> lbls{labels.begin(), labels.size()};
    return gauge_instance<ValueType>(prefix, name, lbls, helptext, unit,
                                     is_sum);
  }

  /// Returns a gauge metric singleton, i.e., the single instance of a
  /// family without label dimensions. Creates all objects lazily if
  /// necessary, but fails if the full name already belongs to a different
  /// family.
  /// @param prefix The prefix (namespace) this family belongs to. Usually the
  ///               application or protocol name, e.g., `http`. The prefix `caf`
  ///               as well as prefixes starting with an underscore are
  ///               reserved.
  /// @param name The human-readable name of the metric, e.g., `requests`.
  /// @param helptext Short explanation of the metric.
  /// @param unit Unit of measurement. Please use base units such as `bytes`
  ///             or `seconds` (prefer lowercase). The pseudo-unit `1`
  ///             identifies dimensionless counts.
  /// @param is_sum Setting this to `true` indicates that this metric adds
  ///               something up to a total, where only the total value is of
  ///               interest. For example, the total number of HTTP requests.
  template <class ValueType = int64_t>
  gauge<ValueType>*
  gauge_singleton(std::string_view prefix, std::string_view name,
                  std::string_view helptext, std::string_view unit = "1",
                  bool is_sum = false) {
    span_t<std::string_view> lbls;
    auto fptr = gauge_family<ValueType>(prefix, name, lbls, helptext, unit,
                                        is_sum);
    return fptr->get_or_add({});
  }

  /// Returns a counter metric family. Creates the family lazily if
  /// necessary, but fails if the full name already belongs to a different
  /// family.
  /// @param prefix The prefix (namespace) this family belongs to. Usually
  ///               the application or protocol name, e.g., `http`. The prefix
  ///               `caf` as well as prefixes starting with an underscore are
  ///               reserved.
  /// @param name The human-readable name of the metric, e.g., `requests`.
  /// @param labels Names for all label dimensions of the metric.
  /// @param helptext Short explanation of the metric.
  /// @param unit Unit of measurement. Please use base units such as `bytes` or
  ///             `seconds` (prefer lowercase). The pseudo-unit `1` identifies
  ///             dimensionless counts.
  /// @param is_sum Setting this to `true` indicates that this metric adds
  ///               something up to a total, where only the total value is of
  ///               interest. For example, the total number of HTTP requests.
  template <class ValueType = int64_t>
  metric_family_impl<counter<ValueType>>*
  counter_family(std::string_view prefix, std::string_view name,
                 span_t<std::string_view> labels, std::string_view helptext,
                 std::string_view unit = "1", bool is_sum = false) {
    using counter_type = counter<ValueType>;
    using family_type = metric_family_impl<counter_type>;
    std::unique_lock<std::mutex> guard{families_mx_};
    if (auto ptr = fetch(prefix, name)) {
      assert_properties(ptr, counter_type::runtime_type, labels, unit, is_sum);
      return static_cast<family_type*>(ptr);
    }
    auto ptr = std::make_unique<family_type>(
      std::string{prefix}, std::string{name}, to_sorted_vec(labels),
      std::string{helptext}, std::string{unit}, is_sum);
    auto result = ptr.get();
    families_.emplace_back(std::move(ptr));
    return result;
  }

  /// @copydoc counter_family
  template <class ValueType = int64_t>
  metric_family_impl<counter<ValueType>>*
  counter_family(std::string_view prefix, std::string_view name,
                 std::initializer_list<std::string_view> labels,
                 std::string_view helptext, std::string_view unit = "1",
                 bool is_sum = false) {
    auto lbl_span = make_span(labels.begin(), labels.size());
    return counter_family<ValueType>(prefix, name, lbl_span, helptext, unit,
                                     is_sum);
  }

  /// Returns a counter. Creates all objects lazily if necessary, but fails
  /// if the full name already belongs to a different family.
  /// @param prefix The prefix (namespace) this family belongs to. Usually the
  ///               application or protocol name, e.g., `http`. The prefix `caf`
  ///               as well as prefixes starting with an underscore are
  ///               reserved.
  /// @param name The human-readable name of the metric, e.g., `requests`.
  /// @param labels Values for all label dimensions of the metric.
  /// @param helptext Short explanation of the metric.
  /// @param unit Unit of measurement. Please use base units such as `bytes` or
  ///             `seconds` (prefer lowercase). The pseudo-unit `1` identifies
  ///             dimensionless counts.
  /// @param is_sum Setting this to `true` indicates that this metric adds
  ///               something up to a total, where only the total value is of
  ///               interest. For example, the total number of HTTP requests.
  template <class ValueType = int64_t>
  counter<ValueType>*
  counter_instance(std::string_view prefix, std::string_view name,
                   span_t<label_view> labels, std::string_view helptext,
                   std::string_view unit = "1", bool is_sum = false) {
    auto label_names = get_label_names(labels);
    auto fptr = counter_family<ValueType>(prefix, name, label_names, helptext,
                                          unit, is_sum);
    return fptr->get_or_add(labels);
  }

  /// @copydoc counter_instance
  template <class ValueType = int64_t>
  counter<ValueType>*
  counter_instance(std::string_view prefix, std::string_view name,
                   std::initializer_list<label_view> labels,
                   std::string_view helptext, std::string_view unit = "1",
                   bool is_sum = false) {
    span_t<label_view> lbls{labels.begin(), labels.size()};
    return counter_instance<ValueType>(prefix, name, lbls, helptext, unit,
                                       is_sum);
  }

  /// Returns a counter metric singleton, i.e., the single instance of a
  /// family without label dimensions. Creates all objects lazily if
  /// necessary, but fails if the full name already belongs to a different
  /// family.
  /// @param prefix The prefix (namespace) this family belongs to. Usually the
  ///               application or protocol name, e.g., `http`. The prefix `caf`
  ///               as well as prefixes starting with an underscore are
  ///               reserved.
  /// @param name The human-readable name of the metric, e.g., `requests`.
  /// @param helptext Short explanation of the metric.
  /// @param unit Unit of measurement. Please use base units such as `bytes`
  ///             or `seconds` (prefer lowercase). The pseudo-unit `1`
  ///             identifies dimensionless counts.
  /// @param is_sum Setting this to `true` indicates that this metric adds
  ///               something up to a total, where only the total value is of
  ///               interest. For example, the total number of HTTP requests.
  template <class ValueType = int64_t>
  counter<ValueType>*
  counter_singleton(std::string_view prefix, std::string_view name,
                    std::string_view helptext, std::string_view unit = "1",
                    bool is_sum = false) {
    span_t<std::string_view> lbls;
    auto fptr = counter_family<ValueType>(prefix, name, lbls, helptext, unit,
                                          is_sum);
    return fptr->get_or_add({});
  }

  /// Returns a histogram metric family. Creates the family lazily if
  /// necessary, but fails if the full name already belongs to a different
  /// family.
  /// @param prefix The prefix (namespace) this family belongs to. Usually the
  ///               application or protocol name, e.g., `http`. The prefix `caf`
  ///               as well as prefixes starting with an underscore are
  ///               reserved.
  /// @param name The human-readable name of the metric, e.g., `requests`.
  /// @param label_names Names for all label dimensions of the metric.
  /// @param default_upper_bounds Upper bounds for the metric buckets.
  /// @param helptext Short explanation of the metric.
  /// @param unit Unit of measurement. Please use base units such as `bytes` or
  ///             `seconds` (prefer lowercase). The pseudo-unit `1` identifies
  ///             dimensionless counts.
  /// @param is_sum Setting this to `true` indicates that this metric adds
  ///               something up to a total, where only the total value is of
  ///               interest. For example, the total number of HTTP requests.
  /// @note The first call wins when calling this function multiple times with
  ///       different bucket settings. Later calls skip checking the bucket
  ///       settings, mainly because this check would be rather expensive.
  /// @note The actor system config may override `upper_bounds`.
  template <class ValueType = int64_t>
  metric_family_impl<histogram<ValueType>>*
  histogram_family(std::string_view prefix, std::string_view name,
                   span_t<std::string_view> label_names,
                   span_t<ValueType> default_upper_bounds,
                   std::string_view helptext, std::string_view unit = "1",
                   bool is_sum = false) {
    using histogram_type = histogram<ValueType>;
    using family_type = metric_family_impl<histogram_type>;
    using upper_bounds_list = std::vector<ValueType>;
    if (default_upper_bounds.empty())
      CAF_RAISE_ERROR("at least one bucket must exist in the default settings");
    std::unique_lock<std::mutex> guard{families_mx_};
    if (auto ptr = fetch(prefix, name)) {
      assert_properties(ptr, histogram_type::runtime_type, label_names, unit,
                        is_sum);
      return static_cast<family_type*>(ptr);
    }
    const settings* sub_settings = nullptr;
    upper_bounds_list upper_bounds;
    if (config_ != nullptr) {
      if (auto grp = get_if<settings>(config_, prefix)) {
        if (sub_settings = get_if<settings>(grp, name);
            sub_settings != nullptr) {
          if (auto lst = get_as<upper_bounds_list>(*sub_settings, "buckets")) {
            std::sort(lst->begin(), lst->end());
            lst->erase(std::unique(lst->begin(), lst->end()), lst->end());
            if (!lst->empty())
              upper_bounds = std::move(*lst);
          }
        }
      }
    }
    if (upper_bounds.empty())
      upper_bounds.assign(default_upper_bounds.begin(),
                          default_upper_bounds.end());
    auto ptr = std::make_unique<family_type>(
      sub_settings, std::string{prefix}, std::string{name},
      to_sorted_vec(label_names), std::string{helptext}, std::string{unit},
      is_sum, std::move(upper_bounds));
    auto result = ptr.get();
    families_.emplace_back(std::move(ptr));
    return result;
  }

  /// @copydoc gauge_family
  template <class ValueType = int64_t>
  metric_family_impl<histogram<ValueType>>*
  histogram_family(std::string_view prefix, std::string_view name,
                   std::initializer_list<std::string_view> label_names,
                   span_t<ValueType> upper_bounds, std::string_view helptext,
                   std::string_view unit = "1", bool is_sum = false) {
    auto lbl_span = make_span(label_names.begin(), label_names.size());
    return histogram_family<ValueType>(prefix, name, lbl_span, upper_bounds,
                                       helptext, unit, is_sum);
  }

  /// Returns a histogram. Creates the family lazily if necessary,
  /// but fails if the full name already belongs to a different
  /// family.
  /// @param prefix The prefix (namespace) this family belongs to. Usually the
  ///               application or protocol name, e.g., `http`. The prefix `caf`
  ///               as well as prefixes starting with an underscore are
  ///               reserved.
  /// @param name The human-readable name of the metric, e.g.,
  ///             `requests`.
  /// @param labels Names for all label dimensions of the metric.
  /// @param upper_bounds Upper bounds for the metric buckets.
  /// @param helptext Short explanation of the metric.
  /// @param unit Unit of measurement. Please use base units such as`bytes` or
  ///             `seconds` (prefer lowercase). The pseudo-unit `1` identifies
  ///             dimensionless counts.
  /// @param is_sum Setting this to `true` indicates that this metric adds
  ///               something up to a total, where only the total value is of
  ///               interest. For example, the total number of HTTP requests.
  /// @note The first call wins when calling this function multiple times with
  ///       different bucket settings. Later calls skip checking the bucket
  ///       settings, mainly because this check would be rather expensive.
  /// @note The actor system config may override `upper_bounds`.
  template <class ValueType = int64_t>
  histogram<ValueType>*
  histogram_instance(std::string_view prefix, std::string_view name,
                     span_t<label_view> labels, span_t<ValueType> upper_bounds,
                     std::string_view helptext, std::string_view unit = "1",
                     bool is_sum = false) {
    auto label_names = get_label_names(labels);
    auto fptr = histogram_family<ValueType>(prefix, name, label_names,
                                            upper_bounds, helptext, unit,
                                            is_sum);
    return fptr->get_or_add(labels);
  }

  /// @copdoc histogram_instance
  template <class ValueType = int64_t>
  histogram<ValueType>*
  histogram_instance(std::string_view prefix, std::string_view name,
                     std::initializer_list<label_view> labels,
                     span_t<ValueType> upper_bounds, std::string_view helptext,
                     std::string_view unit = "1", bool is_sum = false) {
    span_t<label_view> lbls{labels.begin(), labels.size()};
    return histogram_instance<ValueType>(prefix, name, lbls, upper_bounds,
                                         helptext, unit, is_sum);
  }

  /// Returns a histogram metric singleton, i.e., the single instance of a
  /// family without label dimensions. Creates all objects lazily if necessary,
  /// but fails if the full name already belongs to a different family.
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
  /// @note The actor system config may override `upper_bounds`.
  template <class ValueType = int64_t>
  histogram<ValueType>*
  histogram_singleton(std::string_view prefix, std::string_view name,
                      span_t<ValueType> upper_bounds, std::string_view helptext,
                      std::string_view unit = "1", bool is_sum = false) {
    span_t<std::string_view> lbls;
    auto fptr = histogram_family<ValueType>(prefix, name, lbls, upper_bounds,
                                            helptext, unit, is_sum);
    return fptr->get_or_add({});
  }

  /// @internal
  void config(const settings* ptr) {
    config_ = ptr;
  }

  // -- observers --------------------------------------------------------------

  template <class Collector>
  void collect(Collector& collector) const {
    auto f = [&](auto* ptr) { ptr->collect(collector); };
    std::unique_lock<std::mutex> guard{families_mx_};
    for (auto& ptr : families_)
      visit_family(f, ptr.get());
  }

  // -- static utility functions -----------------------------------------------

  /// Returns a pointer to the metric registry from the actor system.
  static metric_registry* from(actor_system& sys);

  // -- modifiers --------------------------------------------------------------

  /// Takes ownership of all metric families in `other`.
  /// @pre `other` *must not* contain any duplicated metric family
  void merge(metric_registry& other);

private:
  /// @pre `families_mx_` is locked.
  metric_family* fetch(const std::string_view& prefix,
                       const std::string_view& name);

  static std::vector<std::string_view> get_label_names(span_t<label_view> xs);

  static std::vector<std::string> to_sorted_vec(span_t<std::string_view> xs);

  static std::vector<std::string> to_sorted_vec(span_t<label_view> xs);

  template <class F>
  static auto visit_family(F& f, const metric_family* ptr) {
    switch (ptr->type()) {
      case metric_type::dbl_counter:
        return f(static_cast<const metric_family_impl<dbl_counter>*>(ptr));
      case metric_type::int_counter:
        return f(static_cast<const metric_family_impl<int_counter>*>(ptr));
      case metric_type::dbl_gauge:
        return f(static_cast<const metric_family_impl<dbl_gauge>*>(ptr));
      case metric_type::int_gauge:
        return f(static_cast<const metric_family_impl<int_gauge>*>(ptr));
      case metric_type::dbl_histogram:
        return f(static_cast<const metric_family_impl<dbl_histogram>*>(ptr));
      default:
        CAF_ASSERT(ptr->type() == metric_type::int_histogram);
        return f(static_cast<const metric_family_impl<int_histogram>*>(ptr));
    }
  }

  void assert_properties(const metric_family* ptr, metric_type type,
                         span_t<std::string_view> label_names,
                         std::string_view unit, bool is_sum);

  void assert_properties(const metric_family* ptr, metric_type type,
                         span_t<label_view> label_names, std::string_view unit,
                         bool is_sum);

  mutable std::mutex families_mx_;
  std::vector<std::unique_ptr<metric_family>> families_;
  const caf::settings* config_;
};

} // namespace caf::telemetry
