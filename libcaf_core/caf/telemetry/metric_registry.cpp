// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/telemetry/metric_registry.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/detail/concepts.hpp"
#include "caf/detail/format.hpp"
#include "caf/raise_error.hpp"
#include "caf/telemetry/metric_family_impl.hpp"
#include "caf/telemetry/metric_type.hpp"
#include "caf/timespan.hpp"

#include <algorithm>
#include <chrono>
#include <thread>

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

metric_registry* metric_registry::from(actor_system& sys) {
  return &sys.metrics();
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
  if (auto i = std::ranges::find_if(families_, eq); i != families_.end())
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
metric_registry::to_sorted_vec(span_t<const std::string_view> xs) {
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
metric_registry::to_sorted_vec(span_t<const label_view> xs) {
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
  span_t<const std::string_view> label_names, std::string_view unit,
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
                                        span_t<const label_view> labels,
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

namespace {

bool labels_match(std::span<const label_view> want,
                  std::span<const label> found) {
  if (want.size() != found.size())
    return false;
  for (const auto& lbl : want) {
    auto is_match = [&lbl](const label& other) {
      return lbl.name() == other.name() && lbl.value() == other.value();
    };
    if (std::ranges::none_of(found, is_match))
      return false;
  }
  return true;
}

using fetch_metric_result
  = std::variant<std::monostate, const int_counter*, const dbl_counter*,
                 const int_gauge*, const dbl_gauge*>;

fetch_metric_result fetch_metric(const metric_registry* reg,
                                 std::string_view prefix, std::string_view name,
                                 std::span<const label_view> labels) {
  fetch_metric_result result;
  auto collector = [prefix, name, labels,
                    &result]<class Wrapped>(const metric_family* family,
                                            const metric* instance,
                                            const Wrapped* wrapped) {
    if constexpr (detail::one_of<Wrapped, int_counter, dbl_counter, int_gauge,
                                 dbl_gauge>) {
      if (family->prefix() == prefix && family->name() == name
          && labels_match(labels, instance->labels())) {
        result = wrapped;
      }
    }
  };
  reg->collect(collector);
  return result;
}

} // namespace

bool metric_registry::wait_for_impl(std::string_view prefix,
                                    std::string_view name,
                                    std::span<const label_view> labels,
                                    timespan rel_timeout,
                                    timespan poll_interval,
                                    callback<bool(int64_t)>& int_pred,
                                    callback<bool(double)>& dbl_pred) const {
  auto pred = [&int_pred, &dbl_pred]<class T>(T x) -> bool {
    if constexpr (std::is_same_v<T, int64_t>) {
      return int_pred(x);
    } else {
      static_assert(std::is_same_v<T, double>);
      return dbl_pred(x);
    }
  };
  auto pred_fwd = [&pred]<class T>(T arg) -> bool {
    if constexpr (std::is_pointer_v<T>) {
      return pred(arg->value());
    } else {
      return false;
    }
  };
  auto deadline = std::chrono::steady_clock::now() + rel_timeout;
  for (;;) {
    auto maybe_metric = fetch_metric(this, prefix, name, labels);
    if (std::visit(pred_fwd, maybe_metric)) {
      return true;
    }
    if (std::chrono::steady_clock::now() >= deadline) {
      return false;
    }
    std::this_thread::sleep_for(
      std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        poll_interval));
  }
}

} // namespace caf::telemetry
