// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/telemetry/metric_registry.hpp"

#include "caf/actor_system_config.hpp"
#include "caf/config.hpp"
#include "caf/raise_error.hpp"
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
  config_ = get_if<settings>(&cfg, "caf.metrics");
}

metric_registry::~metric_registry() {
  // nop
}

void metric_registry::merge(metric_registry& other) {
  if (this == &other)
    return;
  std::unique_lock<std::mutex> guard1{families_mx_, std::defer_lock};
  std::unique_lock<std::mutex> guard2{other.families_mx_, std::defer_lock};
  std::lock(guard1, guard2);
  families_.reserve(families_.size() + other.families_.size());
  for (auto& fptr : other.families_)
    if (fetch(fptr->prefix(), fptr->name()) != nullptr)
      CAF_RAISE_ERROR("failed to merge metrics: duplicated family found");
  families_.insert(families_.end(),
                   std::make_move_iterator(other.families_.begin()),
                   std::make_move_iterator(other.families_.end()));
  other.families_.clear();
}

metric_family* metric_registry::fetch(const std::string_view& prefix,
                                      const std::string_view& name) {
  auto eq = [&](const auto& ptr) {
    return ptr->prefix() == prefix && ptr->name() == name;
  };
  if (auto i = std::find_if(families_.begin(), families_.end(), eq);
      i != families_.end())
    return i->get();
  return nullptr;
}

std::vector<std::string_view>
metric_registry::get_label_names(span_t<label_view> xs) {
  std::vector<std::string_view> result;
  result.reserve(xs.size());
  for (auto& x : xs)
    result.push_back(x.name());
  return result;
}

std::vector<std::string>
metric_registry::to_sorted_vec(span<const std::string_view> xs) {
  std::vector<std::string> result;
  if (!xs.empty()) {
    result.reserve(xs.size());
    for (auto x : xs)
      result.emplace_back(std::string{x});
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
      result.emplace_back(std::string{x.name()});
    std::sort(result.begin(), result.end());
  }
  return result;
}

void metric_registry::assert_properties(
  const metric_family* ptr, metric_type type,
  span<const std::string_view> label_names, std::string_view unit,
  bool is_sum) {
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
  bool operator()(std::string_view x, std::string_view y) const noexcept {
    return x == y;
  }

  bool operator()(std::string_view x, const label_view& y) const noexcept {
    return x == y.name();
  }

  bool operator()(const label_view& x, std::string_view y) const noexcept {
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
                                        std::string_view unit, bool is_sum) {
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
