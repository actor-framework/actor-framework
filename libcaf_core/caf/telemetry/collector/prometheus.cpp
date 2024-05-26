// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/telemetry/collector/prometheus.hpp"

#include "caf/detail/assert.hpp"
#include "caf/telemetry/dbl_gauge.hpp"
#include "caf/telemetry/int_gauge.hpp"
#include "caf/telemetry/metric.hpp"
#include "caf/telemetry/metric_family.hpp"
#include "caf/telemetry/metric_registry.hpp"

#include <cmath>
#include <ctime>
#include <type_traits>

using namespace std::literals;

namespace caf::telemetry::collector {

namespace {

/// Milliseconds since epoch.
struct ms_timestamp {
  int64_t value;

  /// Converts seconds-since-epoch to milliseconds-since-epoch
  explicit ms_timestamp(timestamp from) noexcept {
    using ms_dur = std::chrono::duration<int64_t, std::milli>;
    value = std::chrono::duration_cast<ms_dur>(from.time_since_epoch()).count();
  }

  ms_timestamp(const ms_timestamp&) noexcept = default;

  ms_timestamp& operator=(const ms_timestamp&) noexcept = default;
};

// Converts separators such as '.' and '-' to underlines to follow the
// Prometheus naming conventions.
struct separator_to_underline {
  std::string_view str;
};

void append(prometheus::char_buffer&) {
  // End of recursion.
}

template <class... Ts>
void append(prometheus::char_buffer&, std::string_view, Ts&&...);

template <class... Ts>
void append(prometheus::char_buffer&, separator_to_underline, Ts&&...);

template <class... Ts>
void append(prometheus::char_buffer&, char, Ts&&...);

template <class... Ts>
void append(prometheus::char_buffer&, double, Ts&&...);

template <class T, class... Ts>
std::enable_if_t<std::is_integral_v<T>>
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
void append(prometheus::char_buffer& buf, std::string_view str, Ts&&... xs) {
  buf.insert(buf.end(), str.begin(), str.end());
  append(buf, std::forward<Ts>(xs)...);
}

template <class... Ts>
void append(prometheus::char_buffer& buf, separator_to_underline x,
            Ts&&... xs) {
  for (auto c : x.str) {
    switch (c) {
      default:
        buf.emplace_back(c);
        break;
      case '-':
      case '.':
        buf.emplace_back('_');
    }
  }
  append(buf, std::forward<Ts>(xs)...);
}

template <class... Ts>
void append(prometheus::char_buffer& buf, char ch, Ts&&... xs) {
  buf.emplace_back(ch);
  append(buf, std::forward<Ts>(xs)...);
}

template <class... Ts>
void append(prometheus::char_buffer& buf, double val, Ts&&... xs) {
  if (std::isnan(val)) {
    append(buf, "NaN"sv);
  } else if (std::isinf(val)) {
    if (std::signbit(val))
      append(buf, "+Inf"sv);
    else
      append(buf, "-Inf"sv);
  } else {
    append(buf, std::to_string(val));
  }
  append(buf, std::forward<Ts>(xs)...);
}

template <class T, class... Ts>
std::enable_if_t<std::is_integral_v<T>>
append(prometheus::char_buffer& buf, T val, Ts&&... xs) {
  append(buf, std::to_string(val));
  append(buf, std::forward<Ts>(xs)...);
}

template <class... Ts>
void append(prometheus::char_buffer& buf, const metric_family* family,
            Ts&&... xs) {
  append(buf, separator_to_underline{family->prefix()}, '_',
         separator_to_underline{family->name()});
  if (family->unit() != "1"sv)
    append(buf, '_', family->unit());
  if (family->is_sum())
    append(buf, "_total"sv);
  append(buf, std::forward<Ts>(xs)...);
}

template <class... Ts>
void append(prometheus::char_buffer& buf, const std::vector<label>& labels,
            Ts&&... xs) {
  if (!labels.empty()) {
    append(buf, '{');
    auto i = labels.begin();
    append(buf, separator_to_underline{i->name()}, "=\""sv, i->value(), '"');
    while (++i != labels.end())
      append(buf, ',', separator_to_underline{i->name()}, "=\""sv, i->value(),
             '"');
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

// -- properties ---------------------------------------------------------------

void prometheus::reset() {
  buf_.clear();
  last_scrape_ = timestamp{timespan{0}};
  family_info_.clear();
  histogram_info_.clear();
  current_family_ = nullptr;
  min_scrape_interval_ = timespan{0};
}

// -- scraping API -------------------------------------------------------------

bool prometheus::begin_scrape(timestamp now) {
  if (buf_.empty() || last_scrape_ + min_scrape_interval_ <= now) {
    buf_.clear();
    last_scrape_ = now;
    current_family_ = nullptr;
    return true;
  } else {
    return false;
  }
}

void prometheus::end_scrape() {
  // nop
}

// -- appending into the internal buffer ---------------------------------------

void prometheus::append_histogram(
  const metric_family* family, const metric* instance,
  span<const int_histogram::bucket_type> buckets, int64_t sum) {
  append_histogram_impl(family, instance, buckets, sum);
}

void prometheus::append_histogram(
  const metric_family* family, const metric* instance,
  span<const dbl_histogram::bucket_type> buckets, double sum) {
  append_histogram_impl(family, instance, buckets, sum);
}

// -- collect API --------------------------------------------------------------

std::string_view prometheus::collect_from(const metric_registry& registry,
                                          timestamp now) {
  if (begin_scrape(now)) {
    registry.collect(*this);
    end_scrape();
  }
  return str();
}

// -- implementation details ---------------------------------------------------

void prometheus::set_current_family(const metric_family* family,
                                    std::string_view prometheus_type) {
  if (current_family_ == family)
    return;
  current_family_ = family;
  auto i = family_info_.find(family);
  if (i == family_info_.end()) {
    i = family_info_.emplace(family, char_buffer{}).first;
    if (!family->helptext().empty())
      append(i->second, "# HELP ", family, ' ', family->helptext(), '\n');
    append(i->second, "# TYPE ", family, ' ', prometheus_type, '\n');
  }
  buf_.insert(buf_.end(), i->second.begin(), i->second.end());
}

void prometheus::append_impl(const metric_family* family,
                             std::string_view prometheus_type,
                             const metric* instance, int64_t value) {
  set_current_family(family, prometheus_type);
  append(buf_, family, instance, ' ', value, ' ', ms_timestamp{last_scrape_},
         '\n');
}

void prometheus::append_impl(const metric_family* family,
                             std::string_view prometheus_type,
                             const metric* instance, double value) {
  set_current_family(family, prometheus_type);
  append(buf_, family, instance, ' ', value, ' ', ms_timestamp{last_scrape_},
         '\n');
}

namespace {

template <class BucketType>
auto make_histogram_info(const metric_family* family, const metric* instance,
                         span<const BucketType> buckets) {
  std::vector<prometheus::char_buffer> result;
  auto add_result = [&](auto&&... xs) {
    result.emplace_back();
    append(result.back(), std::forward<decltype(xs)>(xs)...);
  };
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

template <class BucketType, class ValueType>
void prometheus::append_histogram_impl(const metric_family* family,
                                       const metric* instance,
                                       span<const BucketType> buckets,
                                       ValueType sum) {
  auto i = histogram_info_.find(instance);
  if (i == histogram_info_.end()) {
    auto info = make_histogram_info(family, instance, buckets);
    i = histogram_info_.emplace(instance, std::move(info)).first;
  }
  set_current_family(family, "histogram");
  auto& vm = i->second;
  auto acc = ValueType{0};
  auto index = size_t{0};
  for (; index < buckets.size(); ++index) {
    acc += buckets[index].count.value();
    append(buf_, vm[index], acc, ' ', ms_timestamp{last_scrape_}, '\n');
  }
  append(buf_, vm[index++], sum, ' ', ms_timestamp{last_scrape_}, '\n');
  append(buf_, vm[index++], acc, ' ', ms_timestamp{last_scrape_}, '\n');
}

} // namespace caf::telemetry::collector
