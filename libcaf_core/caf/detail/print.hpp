// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/chrono.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/none.hpp"

#include <chrono>
#include <cstdint>
#include <ctime>
#include <limits>
#include <string_view>
#include <type_traits>

namespace caf::detail {

/// Wraps an output iterator to provide a `push_back` and `insert` member
/// functions for using the print algorithms.
template <class OutputIterator>
struct print_iterator_adapter {
  using value_type = char;

  OutputIterator pos;

  explicit print_iterator_adapter(OutputIterator iter) : pos(iter) {
    // nop
  }

  struct sentinel {};

  sentinel end() const noexcept {
    return {};
  }

  void push_back(value_type val) {
    *pos++ = val;
  }

  void insert(sentinel, value_type val) {
    *pos++ == val;
  }

  template <class InputIterator>
  void insert(sentinel, InputIterator first, InputIterator last) {
    pos = std::copy(first, last, pos);
  }
};

// TODO: this function is obsolete. Deprecate/remove with future versions.
CAF_CORE_EXPORT
size_t print_timestamp(char* buf, size_t buf_size, time_t ts, size_t ms);

template <class Buffer>
void print_escaped(Buffer& buf, std::string_view str) {
  buf.push_back('"');
  for (auto c : str) {
    switch (c) {
      default:
        buf.push_back(c);
        break;
      case '\\':
        buf.push_back('\\');
        buf.push_back('\\');
        break;
      case '\b':
        buf.push_back('\\');
        buf.push_back('b');
        break;
      case '\f':
        buf.push_back('\\');
        buf.push_back('f');
        break;
      case '\n':
        buf.push_back('\\');
        buf.push_back('n');
        break;
      case '\r':
        buf.push_back('\\');
        buf.push_back('r');
        break;
      case '\t':
        buf.push_back('\\');
        buf.push_back('t');
        break;
      case '\v':
        buf.push_back('\\');
        buf.push_back('v');
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
void print_unescaped(Buffer& buf, std::string_view str) {
  buf.reserve(buf.size() + str.size());
  auto i = str.begin();
  auto e = str.end();
  while (i != e) {
    switch (*i) {
      default:
        buf.push_back(*i);
        ++i;
        break;
      case '\\':
        if (++i != e) {
          switch (*i) {
            case '"':
              buf.push_back('"');
              break;
            case '\\':
              buf.push_back('\\');
              break;
            case 'b':
              buf.push_back('\b');
              break;
            case 'f':
              buf.push_back('\f');
              break;
            case 'n':
              buf.push_back('\n');
              break;
            case 'r':
              buf.push_back('\r');
              break;
            case 't':
              buf.push_back('\t');
              break;
            case 'v':
              buf.push_back('\v');
              break;
            default:
              buf.push_back('?');
          }
          ++i;
        }
    }
  }
}

template <class Buffer>
void print(Buffer& buf, none_t) {
  using namespace std::literals;
  auto str = "null"sv;
  buf.insert(buf.end(), str.begin(), str.end());
}

template <class Buffer>
void print(Buffer& buf, bool x) {
  using namespace std::literals;
  auto str = x ? "true"sv : "false"sv;
  buf.insert(buf.end(), str.begin(), str.end());
}

template <class Buffer, class T>
std::enable_if_t<std::is_integral_v<T>> print(Buffer& buf, T x) {
  // An integer can at most have 20 digits (UINT64_MAX).
  char stack_buffer[24];
  char* p = stack_buffer;
  // Convert negative values into positives as necessary.
  if constexpr (std::is_signed_v<T>) {
    if (x == std::numeric_limits<T>::min()) {
      using namespace std::literals;
      // The code below would fail for the smallest value, because this value
      // has no positive counterpart. For example, an int8_t ranges from -128 to
      // 127. Hence, an int8_t cannot represent `abs(-128)`.
      std::string_view result;
      if constexpr (sizeof(T) == 1) {
        result = "-128"sv;
      } else if constexpr (sizeof(T) == 2) {
        result = "-32768"sv;
      } else if constexpr (sizeof(T) == 4) {
        result = "-2147483648"sv;
      } else {
        static_assert(sizeof(T) == 8);
        result = "-9223372036854775808"sv;
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
std::enable_if_t<std::is_floating_point_v<T>> print(Buffer& buf, T x) {
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
  using namespace std::literals;
  if (x.count() == 0) {
    auto str = "0s"sv;
    buf.insert(buf.end(), str.begin(), str.end());
    return;
  }
  auto try_print = [&buf](auto converted, std::string_view suffix) {
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
  auto suffix = "ns"sv;
  buf.insert(buf.end(), suffix.begin(), suffix.end());
}

template <class Buffer, class Duration>
void print(Buffer& buf,
           std::chrono::time_point<std::chrono::system_clock, Duration> x) {
  chrono::print(buf, x);
}

} // namespace caf::detail
