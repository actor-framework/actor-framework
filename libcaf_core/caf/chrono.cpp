// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/config.hpp"

#ifdef CAF_WINDOWS
#  include <time.h>

namespace {

int utc_offset(const tm& time_buf) noexcept {
#  if !defined(WINAPI_FAMILY) || (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP)
  // Call _tzset once to initialize the timezone information.
  static bool unused = [] {
    _tzset();
    return true;
  }();
  CAF_IGNORE_UNUSED(unused);
#  endif
  // Get the timezone offset in seconds.
  long offset = 0;
  _get_timezone(&offset);
  // Add the DST bias if DST is active.
  if (time_buf.tm_isdst) {
    long dstbias = 0;
    _get_dstbias(&dstbias);
    offset += dstbias;
  }
  return static_cast<int>(-offset);
}

void to_localtime(time_t ts, tm& time_buf) noexcept {
  localtime_s(&time_buf, &ts);
}

time_t tm_to_time_t(tm& time_buf) noexcept {
  return _mkgmtime(&time_buf);
}

} // namespace

#else
#  define _GNU_SOURCE
#  include <ctime>
#  include <ratio>

namespace {

int utc_offset(const tm& time_buf) noexcept {
  return time_buf.tm_gmtoff;
}

void to_localtime(time_t ts, tm& time_buf) noexcept {
  localtime_r(&ts, &time_buf);
}

time_t tm_to_time_t(tm& time_buf) noexcept {
  return timegm(&time_buf);
}

} // namespace

#endif

#include "caf/chrono.hpp"
#include "caf/detail/parser/read_timestamp.hpp"
#include "caf/detail/print.hpp"
#include "caf/error.hpp"
#include "caf/expected.hpp"
#include "caf/parser_state.hpp"

namespace {

// Prints a fractional part of a timestamp, i.e., a value between 0 and 999.
// Adds leading zeros.
char* print_localtime_fraction(char* pos, int val) noexcept {
  CAF_ASSERT(val < 1000);
  *pos++ = static_cast<char>((val / 100) + '0');
  *pos++ = static_cast<char>(((val % 100) / 10) + '0');
  *pos++ = static_cast<char>((val % 10) + '0');
  return pos;
}

char* print_localtime_impl(char* buf, size_t buf_size, time_t ts,
                           int ns) noexcept {
  // Max size of a timestamp is 35 bytes: "2019-12-31T23:59:59.111222333+01:00".
  // We need at least 36 bytes to store the timestamp and the null terminator.
  CAF_ASSERT(buf_size >= 36);
  // Read the time into a tm struct and print year plus time using strftime.
  auto time_buf = tm{};
  to_localtime(ts, time_buf);
  auto index = strftime(buf, buf_size, "%FT%T", &time_buf);
  auto pos = buf + index;
  // Add the fractional component.
  if (ns > 0) {
    *pos++ = '.';
    auto ms_val = ns / 1'000'000;
    auto us_val = (ns / 1'000) % 1'000;
    auto ns_val = ns % 1'000;
    pos = print_localtime_fraction(pos, ms_val);
    if (ns_val > 0) {
      pos = print_localtime_fraction(pos, us_val);
      pos = print_localtime_fraction(pos, ns_val);
    } else if (us_val > 0) {
      pos = print_localtime_fraction(pos, us_val);
    }
  }
  // Add the timezone part.
  auto offset = utc_offset(time_buf);
  if (offset == 0) {
    *pos++ = 'Z';
    *pos = '\0';
    return pos;
  }
  if (offset > 0) {
    *pos++ = '+';
  } else {
    *pos++ = '-';
    offset = -offset;
  }
  auto hours = offset / 3600;
  auto minutes = (offset % 3600) / 60;
  *pos++ = static_cast<char>(hours / 10 + '0');
  *pos++ = static_cast<char>(hours % 10 + '0');
  *pos++ = ':';
  *pos++ = static_cast<char>(minutes / 10 + '0');
  *pos++ = static_cast<char>(minutes % 10 + '0');
  *pos = '\0';
  return pos;
}

} // namespace

namespace caf::detail {

char* print_localtime(char* buf, size_t buf_size, time_t secs,
                      int nsecs) noexcept {
  return print_localtime_impl(buf, buf_size, secs, nsecs);
}

} // namespace caf::detail

namespace caf::chrono {

namespace {

constexpr bool in_range(int x, int lower, int upper) noexcept {
  return lower <= x && x <= upper;
}

constexpr bool is_leap_year(int year) noexcept {
  // A leap year can be divided by 4 (2020, 2024 ...)
  // -> but it's *not* if it can be divided by 100 (2100, 2200, ...)
  // -> but it's *always* if it can be divided by 400 (2400, 2800, ...)
  return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

constexpr bool is_valid_day(int day, int month, int year) noexcept {
  if (day < 1)
    return false;
  switch (month) {
    default:
      return false;
    case 1:  // January
    case 3:  // March
    case 5:  // May
    case 7:  // July
    case 8:  // August
    case 10: // October
    case 12: // December
      return day <= 31;
    case 4:  // April
    case 6:  // June
    case 9:  // September
    case 11: // November
      return day <= 30;
    case 2: // February
      return is_leap_year(year) ? day <= 29 : day <= 28;
  }
}

} // namespace

bool datetime::valid() const noexcept {
  return in_range(month, 1, 12)            // Month 1 is January.
         && is_valid_day(day, month, year) // Valid days depend on month & year.
         && in_range(hour, 0, 23)          // Hour: 00 to 23.
         && in_range(minute, 0, 59)        // Minute: 00 to 59.
         && in_range(second, 0, 59)        // Second: 00 to 59.
         && in_range(nanosecond, 0, 999'999'999);
}

bool datetime::equals(const datetime& other) const noexcept {
  // Two timestamps can be equal even if they have different UTC offsets. Hence,
  // we need to normalize them before comparing.
  return to_time_t() == other.to_time_t() && nanosecond == other.nanosecond;
}

time_t datetime::to_time_t() const noexcept {
  auto time_buf = tm{};
  time_buf.tm_year = year - 1900;
  time_buf.tm_mon = month - 1;
  time_buf.tm_mday = day;
  time_buf.tm_hour = hour;
  time_buf.tm_min = minute;
  time_buf.tm_sec = second;
  time_buf.tm_isdst = -1;
  return tm_to_time_t(time_buf) - utc_offset.value_or(0);
}

expected<datetime> datetime::from_string(std::string_view str) {
  datetime tmp;
  if (auto err = parse(str, tmp))
    return err;
  return tmp;
}

// -- free functions -----------------------------------------------------------

error parse(std::string_view str, datetime& dest) {
  string_parser_state ps{str.begin(), str.end()};
  detail::parser::read_timestamp(ps, dest);
  return ps.code;
}

void parse(string_parser_state& ps, datetime& dest) {
  detail::parser::read_timestamp(ps, dest);
}

std::string to_string(const datetime& x) {
  std::string result;
  result.reserve(32);
  auto add_two_digits = [&](int x) {
    if (x < 10)
      result.push_back('0');
    detail::print(result, x);
  };
  auto add_three_digits = [&](int x) {
    if (x < 10)
      result += "00";
    else if (x < 100)
      result.push_back('0');
    detail::print(result, x);
  };
  detail::print(result, x.year);
  result.push_back('-');
  add_two_digits(x.month);
  result.push_back('-');
  add_two_digits(x.day);
  result.push_back('T');
  add_two_digits(x.hour);
  result.push_back(':');
  add_two_digits(x.minute);
  result.push_back(':');
  add_two_digits(x.second);
  if (x.nanosecond > 0) {
    result.push_back('.');
    auto ms = x.nanosecond / 1'000'000;
    add_three_digits(ms);
    if ((x.nanosecond % 1'000'000) > 0) {
      auto us = (x.nanosecond % 1'000'000) / 1'000;
      add_three_digits(us);
      if ((x.nanosecond % 1'000) > 0) {
        auto ns = x.nanosecond % 1'000;
        add_three_digits(ns);
      }
    }
  }
  if (!x.utc_offset)
    return result;
  auto utc_offset = *x.utc_offset;
  if (utc_offset == 0) {
    result.push_back('Z');
    return result;
  }
  if (utc_offset > 0) {
    result.push_back('+');
  } else {
    result.push_back('-');
    utc_offset = -utc_offset;
  }
  add_two_digits(utc_offset / 3600); // hours
  result.push_back(':');
  add_two_digits((utc_offset % 3600) / 60); // minutes
  return result;
}

} // namespace caf::chrono
