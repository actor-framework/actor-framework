// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/fwd.hpp"
#include "caf/parser_state.hpp"

#include <chrono>
#include <ctime>
#include <optional>
#include <string>
#include <string_view>

namespace caf::chrono {

/// Tag type for converting timestamps to strings with a fixed number of
/// fractional digits.
struct fixed {};

} // namespace caf::chrono

namespace caf::detail {

/// The size of the buffer used for formatting. Large enough to hold the longest
/// possible ISO 8601 string.
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
/// @param precision The number of fractional digits to print.
/// @param is_fixed Whether to print a fixed number of digits.
/// @returns A pointer to the null terminator of `buf`.
/// @pre `buf_size >= 36`.
CAF_CORE_EXPORT
char* print_localtime(char* buf, size_t buf_size, time_t secs, int nsecs,
                      int precision, bool is_fixed) noexcept;

/// Maps a resolution (seconds, milliseconds, microseconds, or nanoseconds) to
/// the number of fractional digits (0, 3, 6 or 9).
template <class Resolution>
constexpr int to_precision() noexcept {
  if constexpr (std::is_same_v<Resolution, std::chrono::seconds>) {
    return 0;
  } else if constexpr (std::is_same_v<Resolution, std::chrono::milliseconds>) {
    return 3;
  } else if constexpr (std::is_same_v<Resolution, std::chrono::microseconds>) {
    return 6;
  } else {
    static_assert(std::is_same_v<Resolution, std::chrono::nanoseconds>,
                  "expected seconds, milliseconds, "
                  "microseconds, or nanoseconds");
    return 9;
  }
}

} // namespace caf::detail

namespace caf::chrono {

// TODO: becomes obsolete when switching to C++20.
template <class Duration>
using sys_time = std::chrono::time_point<std::chrono::system_clock, Duration>;

/// Formats `ts` in ISO 8601 format.
/// @tparam Resolution The resolution for the fractional part of the seconds.
/// @tparam Policy Either `fixed` to force a fixed number of fractional digits
///                or `unit_t` (default) to use a variable number.
/// @param ts The time point to format.
/// @returns The formatted string.
template <class Resolution = std::chrono::nanoseconds, class Policy = unit_t,
          class Duration>
std::string to_string(sys_time<Duration> ts) {
  char buf[detail::format_buffer_size];
  auto [secs, nsecs] = detail::split_time_point(ts);
  auto end = detail::print_localtime(buf, sizeof(buf), secs, nsecs,
                                     detail::to_precision<Resolution>(),
                                     std::is_same_v<Policy, fixed>);
  return std::string{buf, end};
}

/// Prints `ts` to `out` in ISO 8601 format.
/// @tparam Resolution The resolution for the fractional part of the seconds.
/// @tparam Policy Either `fixed` to force a fixed number of fractional digits
///                or `unit_t` (default) to use a variable number.
/// @param out The buffer to write to.
/// @param ts The time point to print.
template <class Resolution = std::chrono::nanoseconds, class Policy = unit_t,
          class BufferOrIterator, class Duration>
auto print(BufferOrIterator&& out, sys_time<Duration> ts) {
  char buf[detail::format_buffer_size];
  auto [secs, nsecs] = detail::split_time_point(ts);
  auto end = detail::print_localtime(buf, sizeof(buf), secs, nsecs,
                                     detail::to_precision<Resolution>(),
                                     std::is_same_v<Policy, fixed>);
  if constexpr (detail::has_insert_v<std::decay_t<BufferOrIterator>>) {
    out.insert(out.end(), buf, end);
  } else {
    return std::copy(buf, end, out);
  }
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

  /// Convenience function for converting a string to a `datetime` object.
  static expected<datetime> from_string(std::string_view str);

  /// Converts a local time to a `datetime` object.
  template <class Duration>
  static datetime from_local_time(sys_time<Duration> src) {
    auto [secs, nsecs] = detail::split_time_point(src);
    datetime result;
    result.read_local_time(secs, nsecs);
    return result;
  }

  /// Overrides the current date and time with the values from `x`.
  void value(const datetime& x) noexcept {
    *this = x;
  }

  /// Converts this object to a time_point object with given Duration type.
  /// @pre `valid()` returns `true`.
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

  /// Formats this object in ISO 8601 format.
  /// @tparam Resolution The resolution for the fractional part of the seconds.
  /// @tparam Policy Either `fixed` to force a fixed number of fractional digits
  ///                or `unit_t` (default) to use a variable number.
  template <class Resolution = std::chrono::nanoseconds, class Policy = unit_t>
  std::string to_string() const {
    std::string result;
    result.resize(detail::format_buffer_size);
    auto end = print_to<Resolution, Policy>(result.data());
    result.resize(end - result.data());
    return result;
  }

  /// Converts this object to UTC.
  /// @post `utc_offset == 0`
  void force_utc();

  /// Formats this object in ISO 8601 format and writes the result to `out`.
  /// @tparam Resolution The resolution for the fractional part of the seconds.
  /// @tparam Policy Either `fixed` to force a fixed number of fractional digits
  ///                or `unit_t` (default) to use a variable number.
  template <class Resolution = std::chrono::nanoseconds, class Policy = unit_t,
            class OutputIterator>
  OutputIterator print_to(OutputIterator out) const {
    char buf[detail::format_buffer_size];
    auto end = print_to_buf(buf, detail::to_precision<Resolution>(),
                            std::is_same_v<Policy, fixed>);
    return std::copy(buf, end, out);
  }

private:
  void read_local_time(time_t secs, int nsecs);

  /// Assigns a time_t (UTC) value to this object.
  void assign_utc_secs(time_t secs) noexcept;

  /// Converts this object to a time_t value.
  time_t to_time_t() const noexcept;

  /// Prints this object to a buffer.
  /// @param buf The buffer to write to.
  /// @param precision  The number of digits to print for the fractional part.
  /// @param is_fixed Whether to print a fixed number of digits.
  /// @returns A pointer to the null terminator of `buf`.
  /// @pre `buf_size >= format_buffer_size`.
  char* print_to_buf(char* buf, int precision, bool is_fixed) const noexcept;
};

/// @relates datetime
inline bool operator==(const datetime& x, const datetime& y) noexcept {
  return x.equals(y);
}

/// @relates datetime
inline bool operator!=(const datetime& x, const datetime& y) noexcept {
  return !(x == y);
}

/// Parses a date and time string in ISO 8601 format.
/// @param str The string to parse.
/// @param dest The output parameter for the parsed date and time.
/// @returns An error code if parsing failed.
/// @relates datetime
CAF_CORE_EXPORT error parse(std::string_view str, datetime& dest);

/// Parses a date and time string in ISO 8601 format.
/// @param ps The parser state to use.
/// @param dest The output parameter for the parsed date and time.
/// @relates datetime
CAF_CORE_EXPORT void parse(string_parser_state& ps, datetime& dest);

/// @relates datetime
CAF_CORE_EXPORT std::string to_string(const datetime& x);

} // namespace caf::chrono
