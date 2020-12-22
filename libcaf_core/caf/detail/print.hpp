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

#pragma once

#include "caf/none.hpp"
#include "caf/string_view.hpp"

#include <chrono>
#include <cstdint>
#include <ctime>
#include <limits>
#include <type_traits>

namespace caf::detail {

CAF_CORE_EXPORT
size_t print_timestamp(char* buf, size_t buf_size, time_t ts, size_t ms);

template <class Buffer>
void print_escaped(Buffer& buf, string_view str) {
  buf.push_back('"');
  for (auto c : str) {
    switch (c) {
      default:
        buf.push_back(c);
        break;
      case '\n':
        buf.push_back('\\');
        buf.push_back('n');
        break;
      case '\t':
        buf.push_back('\\');
        buf.push_back('t');
        break;
      case '\\':
        buf.push_back('\\');
        buf.push_back('\\');
        break;
      case '"':
        buf.push_back('\\');
        buf.push_back('"');
        break;
    }
  }
  buf.push_back('"');
}

template <class Buffer>
void print(Buffer& buf, none_t) {
  using namespace caf::literals;
  auto str = "null"_sv;
  buf.insert(buf.end(), str.begin(), str.end());
}

template <class Buffer>
void print(Buffer& buf, bool x) {
  using namespace caf::literals;
  auto str = x ? "true"_sv : "false"_sv;
  buf.insert(buf.end(), str.begin(), str.end());
}

template <class Buffer, class T>
std::enable_if_t<std::is_integral<T>::value> print(Buffer& buf, T x) {
  // An integer can at most have 20 digits (UINT64_MAX).
  char stack_buffer[24];
  char* p = stack_buffer;
  // Convert negative values into positives as necessary.
  if constexpr (std::is_signed<T>::value) {
    if (x == std::numeric_limits<T>::min()) {
      using namespace caf::literals;
      // The code below would fail for the smallest value, because this value
      // has no positive counterpart. For example, an int8_t ranges from -128 to
      // 127. Hence, an int8_t cannot represent `abs(-128)`.
      string_view result;
      if constexpr (sizeof(T) == 1) {
        result = "-128"_sv;
      } else if constexpr (sizeof(T) == 2) {
        result = "-32768"_sv;
      } else if constexpr (sizeof(T) == 4) {
        result = "-2147483648"_sv;
      } else {
        static_assert(sizeof(T) == 8);
        result = "-9223372036854775808"_sv;
      }
      buf.insert(buf.end(), result.begin(), result.end());
      return;
    }
    if (x < 0) {
      buf.push_back('-');
      x = -x;
    }
  }
  // Fill the buffer.
  *p++ = static_cast<char>((x % 10) + '0');
  x /= 10;
  while (x != 0) {
    *p++ = static_cast<char>((x % 10) + '0');
    x /= 10;
  }
  // Now, the buffer holds the string representation in reverse order.
  do {
    buf.push_back(*--p);
  } while (p != stack_buffer);
}

template <class Buffer, class T>
std::enable_if_t<std::is_floating_point<T>::value> print(Buffer& buf, T x) {
  // TODO: Check whether to_chars is available on supported compilers and
  //       re-implement using the new API as soon as possible.
  auto str = std::to_string(x);
  if (str.find('.') != std::string::npos) {
    // Drop trailing zeros.
    while (str.back() == '0')
      str.pop_back();
    // Drop trailing dot as well if we've removed all decimal places.
    if (str.back() == '.')
      str.pop_back();
  }
  buf.insert(buf.end(), str.begin(), str.end());
}

template <class Buffer, class Rep, class Period>
void print(Buffer& buf, std::chrono::duration<Rep, Period> x) {
  using namespace caf::literals;
  if (x.count() == 0) {
    auto str = "1s"_sv;
    buf.insert(buf.end(), str.begin(), str.end());
    return;
  }
  auto try_print = [&buf](auto converted, string_view suffix) {
    if (converted.count() < 1)
      return false;
    print(buf, converted.count());
    buf.insert(buf.end(), suffix.begin(), suffix.end());
    return true;
  };
  namespace sc = std::chrono;
  using hours = sc::duration<double, std::ratio<3600>>;
  using minutes = sc::duration<double, std::ratio<60>>;
  using seconds = sc::duration<double>;
  using milliseconds = sc::duration<double, std::milli>;
  using microseconds = sc::duration<double, std::micro>;
  if (try_print(sc::duration_cast<hours>(x), "h")
      || try_print(sc::duration_cast<minutes>(x), "min")
      || try_print(sc::duration_cast<seconds>(x), "s")
      || try_print(sc::duration_cast<milliseconds>(x), "ms")
      || try_print(sc::duration_cast<microseconds>(x), "us"))
    return;
  auto converted = sc::duration_cast<sc::nanoseconds>(x);
  print(buf, converted.count());
  auto suffix = "ns"_sv;
  buf.insert(buf.end(), suffix.begin(), suffix.end());
}

template <class Buffer, class Duration>
void print(Buffer& buf,
           std::chrono::time_point<std::chrono::system_clock, Duration> x) {
  namespace sc = std::chrono;
  using clock_type = sc::system_clock;
  using clock_timestamp = typename clock_type::time_point;
  using clock_duration = typename clock_type::duration;
  auto tse = x.time_since_epoch();
  clock_timestamp as_sys_time{sc::duration_cast<clock_duration>(tse)};
  auto secs = clock_type::to_time_t(as_sys_time);
  auto msecs = sc::duration_cast<sc::milliseconds>(tse).count() % 1000;
  // We print in ISO 8601 format, e.g., "2020-09-01T15:58:42.372". 32-Bytes are
  // more than enough space.
  char stack_buffer[32];
  auto pos = print_timestamp(stack_buffer, 32, secs, msecs);
  buf.insert(buf.end(), stack_buffer, stack_buffer + pos);
}

} // namespace caf::detail
