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

#include "caf/telemetry/collector/prometheus.hpp"

#include <cmath>
#include <ctime>
#include <type_traits>

#include "caf/telemetry/dbl_gauge.hpp"
#include "caf/telemetry/int_gauge.hpp"
#include "caf/telemetry/metric.hpp"
#include "caf/telemetry/metric_family.hpp"
#include "caf/telemetry/metric_registry.hpp"

using namespace caf::literals;

namespace caf::telemetry::collector {

namespace {

/// Milliseconds since epoch.
struct ms_timestamp {
  int64_t value;

  /// Converts seconds-since-epoch to milliseconds-since-epoch
  explicit ms_timestamp(time_t from) : value(from * int64_t{1000}) {
    // nop
  }

  ms_timestamp(const ms_timestamp&) = default;

  ms_timestamp& operator=(const ms_timestamp&) = default;
};

struct underline_to_hyphen {
  string_view str;
};

void append(prometheus::char_buffer&) {
  // End of recursion.
}

template <class... Ts>
void append(prometheus::char_buffer&, string_view, Ts&&...);

template <class... Ts>
void append(prometheus::char_buffer&, underline_to_hyphen, Ts&&...);

template <class... Ts>
void append(prometheus::char_buffer&, char, Ts&&...);

template <class... Ts>
void append(prometheus::char_buffer&, double, Ts&&...);

template <class T, class... Ts>
std::enable_if_t<std::is_integral<T>::value>
append(prometheus::char_buffer& buf, T val, Ts&&... xs);

template <class... Ts>
void append(prometheus::char_buffer&, const metric_family*, Ts&&...);

template <class... Ts>
void append(prometheus::char_buffer&, const std::vector<label>&, Ts&&...);

template <class... Ts>
void append(prometheus::char_buffer&, const metric*, Ts&&...);

template <class... Ts>
void append(prometheus::char_buffer&, ms_timestamp, Ts&&...);

template <class... Ts>
void append(prometheus::char_buffer&, const prometheus::char_buffer&, Ts&&...);

template <class... Ts>
void append(prometheus::char_buffer& buf, string_view str, Ts&&... xs) {
  buf.insert(buf.end(), str.begin(), str.end());
  append(buf, std::forward<Ts>(xs)...);
}

template <class... Ts>
void append(prometheus::char_buffer& buf, underline_to_hyphen x, Ts&&... xs) {
  for (auto c : x.str)
    buf.emplace_back(c != '-' ? c : '_');
  append(buf, std::forward<Ts>(xs)...);
}

template <class... Ts>
void append(prometheus::char_buffer& buf, char ch, Ts&&... xs) {
  buf.emplace_back(ch);
  append(buf, std::forward<Ts>(xs)...);
}

template <class... Ts>
void append(prometheus::char_buffer& buf, double val, Ts&&... xs) {
  if (isnan(val)) {
    append(buf, "NaN"_sv);
  } else if (isinf(val)) {
    if (std::signbit(val))
      append(buf, "+Inf"_sv);
    else
      append(buf, "-Inf"_sv);
  } else {
    append(buf, std::to_string(val));
  }
  append(buf, std::forward<Ts>(xs)...);
}

template <class T, class... Ts>
std::enable_if_t<std::is_integral<T>::value>
append(prometheus::char_buffer& buf, T val, Ts&&... xs) {
  append(buf, std::to_string(val));
  append(buf, std::forward<Ts>(xs)...);
}

template <class... Ts>
void append(prometheus::char_buffer& buf, const metric_family* family,
            Ts&&... xs) {
  append(buf, underline_to_hyphen{family->prefix()}, '_',
         underline_to_hyphen{family->name()});
  if (family->unit() != "1"_sv)
    append(buf, '_', family->unit());
  if (family->is_sum())
    append(buf, "_total"_sv);
  append(buf, std::forward<Ts>(xs)...);
}

template <class... Ts>
void append(prometheus::char_buffer& buf, const std::vector<label>& labels,
            Ts&&... xs) {
  if (!labels.empty()) {
    append(buf, '{');
    auto i = labels.begin();
    append(buf, i->name(), "=\""_sv, i->value(), '"');
    while (++i != labels.end())
      append(buf, ',', i->name(), "=\"", i->value(), '"');
    append(buf, '}');
  }
  append(buf, std::forward<Ts>(xs)...);
}

template <class... Ts>
void append(prometheus::char_buffer& buf, const metric* instance, Ts&&... xs) {
  append(buf, instance->labels(), std::forward<Ts>(xs)...);
}

template <class... Ts>
void append(prometheus::char_buffer& buf, ms_timestamp ts, Ts&&... xs) {
  append(buf, ts.value);
  append(buf, std::forward<Ts>(xs)...);
}

template <class... Ts>
void append(prometheus::char_buffer& buf, const prometheus::char_buffer& x,
            Ts&&... xs) {
  buf.insert(buf.end(), x.begin(), x.end());
  append(buf, std::forward<Ts>(xs)...);
}

} // namespace

string_view prometheus::collect_from(const metric_registry& registry,
                                     time_t now) {
  if (!buf_.empty() && now - now_ < min_scrape_interval_)
    return {buf_.data(), buf_.size()};
  buf_.clear();
  now_ = now;
  registry.collect(*this);
  current_family_ = nullptr;
  return {buf_.data(), buf_.size()};
}

string_view prometheus::collect_from(const metric_registry& registry) {
  return collect_from(registry, time(NULL));
}

void prometheus::operator()(const metric_family* family, const metric* instance,
                            const dbl_gauge* gauge) {
  set_current_family(family, "gauge");
  append(buf_, family, instance, ' ', gauge->value(), ' ', ms_timestamp{now_},
         '\n');
}

void prometheus::operator()(const metric_family* family, const metric* instance,
                            const int_gauge* gauge) {
  set_current_family(family, "gauge");
  append(buf_, family, instance, ' ', gauge->value(), ' ', ms_timestamp{now_},
         '\n');
}

void prometheus::operator()(const metric_family* family, const metric* instance,
                            const dbl_histogram* val) {
  append_histogram(family, instance, val);
}

void prometheus::operator()(const metric_family* family, const metric* instance,
                            const int_histogram* val) {
  append_histogram(family, instance, val);
}

void prometheus::set_current_family(const metric_family* family,
                                    string_view prometheus_type) {
  if (current_family_ == family)
    return;
  current_family_ = family;
  auto i = meta_info_.find(family);
  if (i == meta_info_.end()) {
    i = meta_info_.emplace(family, char_buffer{}).first;
    if (!family->helptext().empty())
      append(i->second, "# HELP ", family, ' ', family->helptext(), '\n');
    append(i->second, "# TYPE ", family, ' ', prometheus_type, '\n');
  }
  buf_.insert(buf_.end(), i->second.begin(), i->second.end());
}

namespace {

template <class ValueType>
auto make_virtual_metrics(const metric_family* family, const metric* instance,
                          const histogram<ValueType>* val) {
  std::vector<prometheus::char_buffer> result;
  auto add_result = [&](auto&&... xs) {
    result.emplace_back();
    append(result.back(), std::forward<decltype(xs)>(xs)...);
  };
  auto buckets = val->buckets();
  auto num_buckets = buckets.size();
  CAF_ASSERT(num_buckets > 1);
  auto labels = instance->labels();
  labels.emplace_back("le", "");
  result.reserve(num_buckets + 2);
  size_t index = 0;
  // Create bucket variable names for 1..N-1.
  for (; index < num_buckets - 1; ++index) {
    auto upper_bound = std::to_string(buckets[index].upper_bound);
    labels.back().value(upper_bound);
    add_result(family, "_bucket", labels, ' ');
  }
  // The last bucket always sets le="+Inf"
  labels.back().value("+Inf");
  add_result(family, "_bucket", labels, ' ');
  labels.pop_back();
  add_result(family, "_sum", labels, ' ');
  add_result(family, "_count", labels, ' ');
  return result;
}

} // namespace

template <class ValueType>
void prometheus::append_histogram(const metric_family* family,
                                  const metric* instance,
                                  const histogram<ValueType>* val) {
  auto i = virtual_metrics_.find(instance);
  if (i == virtual_metrics_.end()) {
    auto metrics = make_virtual_metrics(family, instance, val);
    i = virtual_metrics_.emplace(instance, std::move(metrics)).first;
  }
  set_current_family(family, "histogram");
  auto& vm = i->second;
  auto buckets = val->buckets();
  size_t index = 0;
  for (; index < buckets.size() - 1; ++index) {
    append(buf_, vm[index], buckets[index].gauge.value(), ' ',
           ms_timestamp{now_}, '\n');
  }
  auto count = buckets[index].gauge.value();
  append(buf_, vm[index], count, ' ', ms_timestamp{now_}, '\n');
  append(buf_, vm[++index], val->sum(), ' ', ms_timestamp{now_}, '\n');
  append(buf_, vm[++index], count, ' ', ms_timestamp{now_}, '\n');
}

} // namespace caf::telemetry::collector
