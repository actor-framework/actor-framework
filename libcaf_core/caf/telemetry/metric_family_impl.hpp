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
  using impl_type = metric_impl<Type>;

  using metric_family::metric_family;

  metric_type type() const noexcept override {
    return Type::runtime_type;
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
      m = metrics_.emplace(m, std::make_unique<impl_type>(std::move(cpy)));
    }
    return std::addressof(m->get()->impl());
  }

  Type* get_or_add(std::initializer_list<label_view> labels) {
    return get_or_add(span<const label_view>{labels.begin(), labels.size()});
  }

  template <class Collector>
  void collect(Collector& collector) const {
    std::unique_lock<std::mutex> guard{mx_};
    for (auto& ptr : metrics_)
      collector(this, ptr.get(), std::addressof(ptr->impl()));
  }

private:
  mutable std::mutex mx_;
  std::vector<std::unique_ptr<impl_type>> metrics_;
};

} // namespace caf::telemetry
