// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <algorithm>
#include <initializer_list>
#include <memory>
#include <mutex>

#include "caf/span.hpp"
#include "caf/string_view.hpp"
#include "caf/telemetry/label.hpp"
#include "caf/telemetry/label_view.hpp"
#include "caf/telemetry/metric.hpp"
#include "caf/telemetry/metric_family.hpp"
#include "caf/telemetry/metric_impl.hpp"

namespace caf::telemetry {

template <class Type>
class metric_family_impl : public metric_family {
public:
  using super = metric_family;

  using impl_type = metric_impl<Type>;

  using extra_setting_type = typename impl_type::family_setting;

  template <class... Ts>
  metric_family_impl(const settings* config, std::string prefix,
                     std::string name, std::vector<std::string> label_names,
                     std::string helptext, std::string unit, bool is_sum,
                     Ts&&... xs)
    : super(Type::runtime_type, std::move(prefix), std::move(name),
            std::move(label_names), std::move(helptext), std::move(unit),
            is_sum),
      config_(config),
      extra_setting_(std::forward<Ts>(xs)...) {
    // nop
  }

  template <class... Ts>
  metric_family_impl(std::string prefix, std::string name,
                     std::vector<std::string> label_names, std::string helptext,
                     std::string unit, bool is_sum, Ts&&... xs)
    : super(Type::runtime_type, std::move(prefix), std::move(name),
            std::move(label_names), std::move(helptext), std::move(unit),
            is_sum),
      config_(nullptr),
      extra_setting_(std::forward<Ts>(xs)...) {
    // nop
  }

  Type* get_or_add(span<const label_view> labels) {
    auto has_label_values = [labels](const auto& metric_ptr) {
      const auto& metric_labels = metric_ptr->labels();
      return std::is_permutation(metric_labels.begin(), metric_labels.end(),
                                 labels.begin(), labels.end());
    };
    std::unique_lock<std::mutex> guard{mx_};
    auto m = std::find_if(metrics_.begin(), metrics_.end(), has_label_values);
    if (m == metrics_.end()) {
      std::vector<label> cpy{labels.begin(), labels.end()};
      std::sort(cpy.begin(), cpy.end());
      std::unique_ptr<impl_type> ptr;
      if constexpr (std::is_same<extra_setting_type, unit_t>::value)
        ptr.reset(new impl_type(std::move(cpy)));
      else
        ptr.reset(new impl_type(std::move(cpy), config_, extra_setting_));
      m = metrics_.emplace(m, std::move(ptr));
    }
    return std::addressof(m->get()->impl());
  }

  Type* get_or_add(std::initializer_list<label_view> labels) {
    return get_or_add(span<const label_view>{labels.begin(), labels.size()});
  }

  // -- properties --

  const extra_setting_type& extra_setting() const noexcept {
    return extra_setting_;
  }

  const settings* config() const noexcept {
    return config_;
  }

  template <class Collector>
  void collect(Collector& collector) const {
    std::unique_lock<std::mutex> guard{mx_};
    for (auto& ptr : metrics_)
      collector(this, ptr.get(), std::addressof(ptr->impl()));
  }

private:
  const settings* config_;
  extra_setting_type extra_setting_;
  mutable std::mutex mx_;
  std::vector<std::unique_ptr<impl_type>> metrics_;
};

} // namespace caf::telemetry
