// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/config.hpp"

#ifdef CAF_WINDOWS
#  include <time.h>

namespace {

int get_utc_offset(const tm& time_buf) noexcept {
#  if !defined(WINAPI_FAMILY) || (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP)
  // Call _tzset once to initialize the timezone information.
  static bool unused = [] {
    _tzset();
    return true;
  }();
  static_cast<void>(unused);
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

void to_local_time(time_t ts, tm& time_buf) noexcept {
  localtime_s(&time_buf, &ts);
}

void to_utc_time(time_t ts, tm& time_buf) noexcept {
  gmtime_s(&time_buf, &ts);
}

time_t tm_to_time_t(tm& time_buf) noexcept {
  return _mkgmtime(&time_buf);
}

} // namespace

#else
#  include <ctime>
#  include <ratio>

namespace {

auto get_utc_offset(const tm& time_buf) noexcept {
  return time_buf.tm_gmtoff;
}

void to_local_time(time_t ts, tm& time_buf) noexcept {
  localtime_r(&ts, &time_buf);
}

void to_utc_time(time_t ts, tm& time_buf) noexcept {
  gmtime_r(&ts, &time_buf);
}

time_t tm_to_time_t(tm& time_buf) noexcept {
  return timegm(&time_buf);
}

} // namespace

#endif

#include "caf/chrono.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/parser/read_timestamp.hpp"
#include "caf/detail/print.hpp"
#include "caf/error.hpp"
#include "caf/expected.hpp"
#include "caf/parser_state.hpp"

namespace caf::detail {

namespace {

// Prints a fractional part of a timestamp, i.e., a value between 0 and 999.
// Adds leading zeros.
char* print_3digits(char* pos, int val) noexcept {
  CAF_ASSERT(val < 1000);
  *pos++ = static_cast<char>((val / 100) + '0');
  *pos++ = static_cast<char>(((val % 100) / 10) + '0');
  *pos++ = static_cast<char>((val % 10) + '0');
  return pos;
}

char* print_fractional_component(char* pos, int ns, int precision,
                                 bool is_fixed) noexcept {
  if (precision > 0 && (ns > 0 || is_fixed)) {
    auto ms_val = ns / 1'000'000;
    auto us_val = (ns / 1'000) % 1'000;
    auto ns_val = ns % 1'000;
    if (is_fixed) {
      *pos++ = '.';
      switch (precision) {
        case 3:
          pos = print_3digits(pos, ms_val);
          break;
        case 6:
          pos = print_3digits(pos, ms_val);
          pos = print_3digits(pos, us_val);
          break;
        default: // chrono::precision::nano:
          pos = print_3digits(pos, ms_val);
          pos = print_3digits(pos, us_val);
          pos = print_3digits(pos, ns_val);
          break;
      }
      return pos;
    }
    // Check what the maximum precision is that we can actually print.
    auto effective_precision = precision;
    if (ns_val > 0 && precision == 9) {
      effective_precision = 9;
    } else if (us_val > 0 && precision >= 6) {
      effective_precision = 6;
    } else if (ms_val > 0 && precision >= 3) {
      effective_precision = 3;
    } else {
      effective_precision = 0;
    }
    // Print the fractional part.
    switch (effective_precision) {
      default: // chrono::precision::unit
        break;
      case 3:
        *pos++ = '.';
        pos = print_3digits(pos, ms_val);
        break;
      case 6:
        *pos++ = '.';
        pos = print_3digits(pos, ms_val);
        pos = print_3digits(pos, us_val);
        break;
      case 9:
        *pos++ = '.';
        pos = print_3digits(pos, ms_val);
        pos = print_3digits(pos, us_val);
        pos = print_3digits(pos, ns_val);
        break;
    }
  }
  return pos;
}

char* print_utf_offset(char* pos, int offset) {
  if (offset == 0) {
    *pos++ = 'Z';
    return pos;
  }
  if (offset > 0) {
    *pos++ = '+';
  } else {
    *pos++ = '-';
    offset = -offset;
  }
  auto add_two_digits = [&](int x) {
    *pos++ = static_cast<char>((x / 10) + '0');
    *pos++ = static_cast<char>((x % 10) + '0');
  };
  add_two_digits(offset / 3600); // hours
  *pos++ = ':';
  add_two_digits((offset % 3600) / 60); // minutes
  return pos;
}

} // namespace

char* print_localtime(char* buf, size_t buf_size, time_t ts, int ns,
                      int precision, bool is_fixed) noexcept {
  // Max size of a timestamp is 35 bytes: "2019-12-31T23:59:59.111222333+01:00".
  // We need at least 36 bytes to store the timestamp and the null terminator.
  CAF_ASSERT(buf_size >= 36);
  // Read the time into a tm struct and print year plus time using strftime.
  auto time_buf = tm{};
  to_local_time(ts, time_buf);
  auto index = strftime(buf, buf_size, "%FT%T", &time_buf);
  auto pos = buf + index;
  // Add fractional part and UTC offset.
  pos = print_fractional_component(pos, ns, precision, is_fixed);
  pos = print_utf_offset(pos, get_utc_offset(time_buf));
  *pos = '\0';
  return pos;
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

void datetime::force_utc() {
  if (!utc_offset) {
    utc_offset = 0;
    return;
  }
  if (utc_offset == 0)
    return;
  auto offset = *utc_offset;
  utc_offset = 0;
  auto ts = to_time_t();
  assign_utc_secs(ts - offset);
}

void datetime::read_local_time(time_t secs, int nsecs) {
  auto time_buf = tm{};
  ::to_local_time(secs, time_buf);
  year = time_buf.tm_year + 1900;
  month = time_buf.tm_mon + 1;
  day = time_buf.tm_mday;
  hour = time_buf.tm_hour;
  minute = time_buf.tm_min;
  second = time_buf.tm_sec;
  nanosecond = nsecs;
  utc_offset = get_utc_offset(time_buf);
}

void datetime::assign_utc_secs(time_t secs) noexcept {
  auto time_buf = tm{};
  to_utc_time(secs, time_buf);
  year = time_buf.tm_year + 1900;
  month = time_buf.tm_mon + 1;
  day = time_buf.tm_mday;
  hour = time_buf.tm_hour;
  minute = time_buf.tm_min;
  second = time_buf.tm_sec;
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

char* datetime::print_to_buf(char* buf, int precision,
                             bool is_fixed) const noexcept {
  detail::print_iterator_adapter result{buf};
  auto add_two_digits = [&result](int x) {
    if (x < 10)
      result.push_back('0');
    detail::print(result, x);
  };
  auto seal = [&result] {
    *result.pos = '\0';
    return result.pos;
  };
  // Generate "YYYY-MM-DDThh:mm:ss".
  detail::print(result, year);
  result.push_back('-');
  add_two_digits(month);
  result.push_back('-');
  add_two_digits(day);
  result.push_back('T');
  add_two_digits(hour);
  result.push_back(':');
  add_two_digits(minute);
  result.push_back(':');
  add_two_digits(second);
  // Add (optional) fractional component and UTC offset.
  result.pos = detail::print_fractional_component(result.pos, nanosecond,
                                                  precision, is_fixed);
  if (!utc_offset)
    return seal();
  result.pos = detail::print_utf_offset(result.pos, *utc_offset);
  return seal();
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

std::string to_string(const datetime& dest) {
  return dest.to_string();
}

} // namespace caf::chrono
