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

#include "caf/actor_system_config.hpp"
#include "caf/config.hpp"
#include "caf/telemetry/dbl_gauge.hpp"
#include "caf/telemetry/int_gauge.hpp"
#include "caf/telemetry/metric_family_impl.hpp"
#include "caf/telemetry/metric_impl.hpp"
#include "caf/telemetry/metric_type.hpp"

namespace {

template <class Range1, class Range2>
bool equals(Range1&& xs, Range2&& ys) {
  return std::equal(xs.begin(), xs.end(), ys.begin(), ys.end());
}

} // namespace

namespace caf::telemetry {

metric_registry::metric_registry() : config_(nullptr) {
  // nop
}

metric_registry::metric_registry(const actor_system_config& cfg) {
  config_ = get_if<settings>(&cfg, "metrics");
}

metric_registry::~metric_registry() {
  // nop
}

metric_family* metric_registry::fetch(const string_view& prefix,
                                      const string_view& name) {
  auto eq = [&](const auto& ptr) {
    return ptr->prefix() == prefix && ptr->name() == name;
  };
  if (auto i = std::find_if(families_.begin(), families_.end(), eq);
      i != families_.end())
    return i->get();
  return nullptr;
}

std::vector<std::string>
metric_registry::to_sorted_vec(span<const string_view> xs) {
  std::vector<std::string> result;
  if (!xs.empty()) {
    result.reserve(xs.size());
    for (auto x : xs)
      result.emplace_back(to_string(x));
    std::sort(result.begin(), result.end());
  }
  return result;
}

std::vector<std::string>
metric_registry::to_sorted_vec(span<const label_view> xs) {
  std::vector<std::string> result;
  if (!xs.empty()) {
    result.reserve(xs.size());
    for (auto x : xs)
      result.emplace_back(to_string(x.name()));
    std::sort(result.begin(), result.end());
  }
  return result;
}

void metric_registry::assert_properties(const metric_family* ptr,
                                        metric_type type,
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

namespace {

struct label_name_eq {
  bool operator()(string_view x, string_view y) const noexcept {
    return x == y;
  }

  bool operator()(string_view x, const label_view& y) const noexcept {
    return x == y.name();
  }

  bool operator()(const label_view& x, string_view y) const noexcept {
    return x.name() == y;
  }

  bool operator()(label_view x, label_view y) const noexcept {
    return x == y;
  }
};

} // namespace

void metric_registry::assert_properties(const metric_family* ptr,
                                        metric_type type,
                                        span<const label_view> labels,
                                        string_view unit, bool is_sum) {
  auto labels_match = [&] {
    const auto& xs = ptr->label_names();
    const auto& ys = labels;
    label_name_eq eq;
    return std::is_sorted(ys.begin(), ys.end())
             ? std::equal(xs.begin(), xs.end(), ys.begin(), ys.end(), eq)
             : std::is_permutation(xs.begin(), xs.end(), ys.begin(), ys.end(),
                                   eq);
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
