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

#include "caf/telemetry/int_gauge.hpp"
#include "caf/telemetry/metric.hpp"
#include "caf/telemetry/metric_family.hpp"
#include "caf/telemetry/metric_registry.hpp"

using namespace caf::literals;

namespace caf::telemetry::collector {

namespace {

void append(prometheus::char_buffer&) {
  // End of recursion.
}

template <class... Ts>
void append(prometheus::char_buffer&, string_view, Ts&&...);

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
void append(prometheus::char_buffer&, const metric*, Ts&&...);

template <class... Ts>
void append(prometheus::char_buffer& buf, string_view str, Ts&&... xs) {
  buf.insert(buf.end(), str.begin(), str.end());
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
    if (signbit(val))
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
  append(buf, family->prefix(), '_', family->name());
  if (family->unit() != "1"_sv)
    append(buf, '_', family->unit());
  if (family->is_sum())
    append(buf, "_total"_sv);
  append(buf, std::forward<Ts>(xs)...);
}

template <class... Ts>
void append(prometheus::char_buffer& buf, const metric* instance, Ts&&... xs) {
  const auto& labels = instance->labels();
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
                            const int_gauge* gauge) {
  set_current_family(family, "gauge");
  append(buf_, family, instance, ' ', gauge->value(), ' ', now_, '\n');
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

} // namespace caf::telemetry::collector
