// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/parser_state.hpp"
#include "caf/string_view.hpp"

#include <chrono>
#include <ctime>
#include <optional>
#include <string>

namespace caf::detail {

/// The size of the buffer used for formatting.
constexpr size_t format_buffer_size = 40;

/// Splits the time point into seconds and nanoseconds since epoch.
template <class TimePoint>
std::pair<time_t, int> split_time_point(TimePoint ts) {
  namespace sc = std::chrono;
  using clock_type = sc::system_clock;
  using clock_timestamp = typename clock_type::time_point;
  using clock_duration = typename clock_type::duration;
  auto tse = ts.time_since_epoch();
  clock_timestamp as_sys_time{sc::duration_cast<clock_duration>(tse)};
  auto secs = clock_type::to_time_t(as_sys_time);
  auto ns = sc::duration_cast<sc::nanoseconds>(tse).count() % 1'000'000'000;
  return {secs, static_cast<int>(ns)};
}

/// Prints the date and time to a buffer.
/// @param buf The buffer to write to.
/// @param buf_size The size of `buf` in bytes.
/// @param secs The number of seconds since epoch.
/// @param nsecs The number of nanoseconds since the last full second.
/// @returns A pointer to the null terminator of `buf`.
/// @pre `buf_size >= 36`.
CAF_CORE_EXPORT
char* print_localtime(char* buf, size_t buf_size, time_t secs,
                      int nsecs) noexcept;

} // namespace caf::detail

namespace caf::chrono {

// TODO: becomes obsolete when switching to C++20.
template <class Duration>
using sys_time = std::chrono::time_point<std::chrono::system_clock, Duration>;

/// Formats `ts` in ISO 8601 format.
/// @param ts The time point to format.
/// @returns The formatted string.
template <class Duration>
std::string to_string(sys_time<Duration> ts) {
  char buf[detail::format_buffer_size];
  auto [secs, nsecs] = detail::split_time_point(ts);
  detail::print_localtime(buf, sizeof(buf), secs, nsecs);
  return buf;
}

/// Prints `ts` to `out` in ISO 8601 format.
/// @param out The buffer to write to.
/// @param ts The time point to print.
template <class Buffer, class Duration>
void print(Buffer& out, sys_time<Duration> ts) {
  char buf[detail::format_buffer_size];
  auto [secs, nsecs] = detail::split_time_point(ts);
  auto end = detail::print_localtime(buf, sizeof(buf), secs, nsecs);
  out.insert(out.end(), buf, end);
}

/// Represents a point in time, expressed as a date and time of day. Also
/// provides formatting and parsing functionality for ISO 8601.
class CAF_CORE_EXPORT datetime {
public:
  /// The year.
  int year = 0;

  /// The month of the year, starting with 1 for January.
  int month = 0;

  /// The day of the month, starting with 1.
  int day = 0;

  /// The hour of the day, starting with 0.
  int hour = 0;

  /// The minute of the hour, starting with 0.
  int minute = 0;

  /// The second of the minute, starting with 0.
  int second = 0;

  /// The nanosecond of the second, starting with 0.
  int nanosecond = 0;

  /// The offset from UTC in seconds.
  std::optional<int> utc_offset;

  /// Returns whether this object contains a valid date and time. A default
  /// constructed object is invalid.
  bool valid() const noexcept;

  /// Returns whether this object is equal to `other`.
  bool equals(const datetime& other) const noexcept;

  /// Parses a date and time string in ISO 8601 format.
  /// @param str The string to parse.
  /// @returns The parsed date and time.
  error parse(string_view str);

  /// Convenience function for converting a string to a `datetime` object.
  static expected<datetime> from_string(string_view str);

  /// Parses a date and time string in ISO 8601 format.
  /// @param ps The parser state to use.
  void parse(string_parser_state& ps);

  /// Overrides the current date and time with the values from `x`.
  void value(const datetime& x) noexcept {
    *this = x;
  }

  /// Converts this object to a time_point object with given Duration type.
  template <class Duration = std::chrono::system_clock::duration>
  auto to_local_time() const noexcept {
    // TODO: consider using year_month_day once we have C++20.
    namespace sc = std::chrono;
    auto converted = sc::system_clock::from_time_t(to_time_t());
    auto result = sc::time_point_cast<Duration>(converted);
    auto ns = sc::nanoseconds{nanosecond};
    result += sc::duration_cast<Duration>(ns);
    return sc::time_point_cast<Duration>(result);
  }

private:
  /// Converts this object to a time_t value.
  time_t to_time_t() const noexcept;
};

/// @relates datetime
inline bool operator==(const datetime& x, const datetime& y) noexcept {
  return x.equals(y);
}

/// @relates datetime
inline bool operator!=(const datetime& x, const datetime& y) noexcept {
  return !(x == y);
}

/// @relates datetime
std::string to_string(const datetime& x);

} // namespace caf::chrono
