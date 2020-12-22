/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
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

#include <cstdint>
#include <cstring>
#include <iterator>
#include <type_traits>
#include <utility>
#include <vector>

#include "caf/detail/core_export.hpp"
#include "caf/detail/squashed_int.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/error.hpp"
#include "caf/fwd.hpp"
#include "caf/message.hpp"
#include "caf/none.hpp"
#include "caf/parser_state.hpp"
#include "caf/string_view.hpp"
#include "caf/unit.hpp"

namespace caf::detail {

// -- utility types ------------------------------------------------------------

enum class time_unit {
  invalid,
  hours,
  minutes,
  seconds,
  milliseconds,
  microseconds,
  nanoseconds,
};

CAF_CORE_EXPORT void parse(string_parser_state& ps, time_unit& x);

struct literal {
  string_view str;
};

CAF_CORE_EXPORT void parse(string_parser_state& ps, literal x);

// -- boolean type -------------------------------------------------------------

CAF_CORE_EXPORT void parse(string_parser_state& ps, bool& x);

// -- signed integer types -----------------------------------------------------

CAF_CORE_EXPORT void parse(string_parser_state& ps, int8_t& x);

CAF_CORE_EXPORT void parse(string_parser_state& ps, int16_t& x);

CAF_CORE_EXPORT void parse(string_parser_state& ps, int32_t& x);

CAF_CORE_EXPORT void parse(string_parser_state& ps, int64_t& x);

// -- unsigned integer types ---------------------------------------------------

CAF_CORE_EXPORT void parse(string_parser_state& ps, uint8_t& x);

CAF_CORE_EXPORT void parse(string_parser_state& ps, uint16_t& x);

CAF_CORE_EXPORT void parse(string_parser_state& ps, uint32_t& x);

CAF_CORE_EXPORT void parse(string_parser_state& ps, uint64_t& x);

// -- non-fixed size integer types ---------------------------------------------

template <class T>
detail::enable_if_t<std::is_integral<T>::value>
parse(string_parser_state& ps, T& x) {
  using squashed_type = squashed_int_t<T>;
  return parse(ps, reinterpret_cast<squashed_type&>(x));
}

// -- floating point types -----------------------------------------------------

CAF_CORE_EXPORT void parse(string_parser_state& ps, float& x);

CAF_CORE_EXPORT void parse(string_parser_state& ps, double& x);

CAF_CORE_EXPORT void parse(string_parser_state& ps, long double& x);

// -- CAF types ----------------------------------------------------------------

CAF_CORE_EXPORT void parse(string_parser_state& ps, ipv4_address& x);

CAF_CORE_EXPORT void parse(string_parser_state& ps, ipv4_subnet& x);

CAF_CORE_EXPORT void parse(string_parser_state& ps, ipv4_endpoint& x);

CAF_CORE_EXPORT void parse(string_parser_state& ps, ipv6_address& x);

CAF_CORE_EXPORT void parse(string_parser_state& ps, ipv6_subnet& x);

CAF_CORE_EXPORT void parse(string_parser_state& ps, ipv6_endpoint& x);

CAF_CORE_EXPORT void parse(string_parser_state& ps, uri& x);

CAF_CORE_EXPORT void parse(string_parser_state& ps, config_value& x);

// -- variadic utility ---------------------------------------------------------

template <class... Ts>
bool parse_sequence(string_parser_state& ps, Ts&&... xs) {
  auto parse_one = [&](auto& x) {
    parse(ps, x);
    return ps.code <= pec::trailing_character;
  };
  return (parse_one(xs) && ...);
}

// -- STL types ----------------------------------------------------------------

CAF_CORE_EXPORT void parse(string_parser_state& ps, std::string& x);

template <class Rep, class Period>
void parse(string_parser_state& ps, std::chrono::duration<Rep, Period>& x) {
  namespace sc = std::chrono;
  using value_type = sc::duration<Rep, Period>;
  auto set = [&x](auto value) { x = sc::duration_cast<value_type>(value); };
  auto count = 0.0;
  auto suffix = time_unit::invalid;
  parse_sequence(ps, count, suffix);
  if (ps.code == pec::success) {
    switch (suffix) {
      case time_unit::hours:
        set(sc::duration<double, std::ratio<3600>>{count});
        break;
      case time_unit::minutes:
        set(sc::duration<double, std::ratio<60>>{count});
        break;
      case time_unit::seconds:
        set(sc::duration<double>{count});
        break;
      case time_unit::milliseconds:
        set(sc::duration<double, std::milli>{count});
        break;
      case time_unit::microseconds:
        set(sc::duration<double, std::micro>{count});
        break;
      case time_unit::nanoseconds:
        set(sc::duration<double, std::nano>{count});
        break;
      default:
        ps.code = pec::invalid_state;
    }
  }
}

template <class Duration>
void parse(string_parser_state& ps,
           std::chrono::time_point<std::chrono::system_clock, Duration>& x) {
  namespace sc = std::chrono;
  using value_type = sc::time_point<sc::system_clock, Duration>;
  // Parse ISO 8601 as generated by detail::print.
  int32_t year = 0;
  int32_t month = 0;
  int32_t day = 0;
  int32_t hour = 0;
  int32_t minute = 0;
  int32_t second = 0;
  int32_t milliseconds = 0;
  if (!parse_sequence(ps,
                      // YYYY-MM-DD
                      year, literal{{"-"}}, month, literal{{"-"}}, day,
                      // "T"
                      literal{{"T"}},
                      // hh:mm:ss
                      hour, literal{{":"}}, minute, literal{{":"}}, second,
                      // "." xxx
                      literal{{"."}}, milliseconds))
    return;
  if (ps.code != pec::success)
    return;
  // Fill tm record.
  tm record;
  record.tm_sec = second;
  record.tm_min = minute;
  record.tm_hour = hour;
  record.tm_mday = day;
  record.tm_mon = month - 1;    // months since January
  record.tm_year = year - 1900; // years since 1900
  record.tm_wday = -1;          // n/a
  record.tm_yday = -1;          // n/a
  record.tm_isdst = -1;         // n/a
  // Convert to chrono time and add the milliseconds.
  auto tstamp = sc::system_clock::from_time_t(mktime(&record));
  auto since_epoch = sc::duration_cast<Duration>(tstamp.time_since_epoch());
  since_epoch += sc::milliseconds{milliseconds};
  x = value_type{since_epoch};
}

// -- container types ----------------------------------------------------------

CAF_CORE_EXPORT void parse_element(string_parser_state& ps, std::string& x,
                                   const char* char_blacklist);

template <class T>
enable_if_t<!is_pair<T>::value>
parse_element(string_parser_state& ps, T& x, const char*);

template <class First, class Second, size_t N>
void parse_element(string_parser_state& ps, std::pair<First, Second>& kvp,
                   const char (&char_blacklist)[N]);

struct require_opening_char_t {};
constexpr auto require_opening_char = require_opening_char_t{};

struct allow_omitting_opening_char_t {};
constexpr auto allow_omitting_opening_char = allow_omitting_opening_char_t{};

template <class T, class Policy = allow_omitting_opening_char_t>
enable_if_tt<is_iterable<T>>
parse(string_parser_state& ps, T& xs, Policy = {}) {
  using value_type = deconst_kvp_t<typename T::value_type>;
  static constexpr auto is_map_type = is_pair<value_type>::value;
  static constexpr auto opening_char = is_map_type ? '{' : '[';
  static constexpr auto closing_char = is_map_type ? '}' : ']';
  auto out = std::inserter(xs, xs.end());
  // List/map using [] or {} notation.
  if (ps.consume(opening_char)) {
    char char_blacklist[] = {closing_char, ',', '\0'};
    do {
      if (ps.consume(closing_char)) {
        ps.skip_whitespaces();
        ps.code = ps.at_end() ? pec::success : pec::trailing_character;
        return;
      }
      value_type tmp;
      parse_element(ps, tmp, char_blacklist);
      if (ps.code > pec::trailing_character)
        return;
      *out++ = std::move(tmp);
    } while (ps.consume(','));
    if (ps.consume(closing_char)) {
      ps.skip_whitespaces();
      ps.code = ps.at_end() ? pec::success : pec::trailing_character;
    } else {
      ps.code = pec::unexpected_character;
    }
    return;
  }
  if constexpr (std::is_same<Policy, require_opening_char_t>::value) {
    ps.code = pec::unexpected_character;
  } else {
    // An empty string simply results in an empty list/map.
    if (ps.at_end())
      return;
    // List/map without [] or {}.
    do {
      char char_blacklist[] = {',', '\0'};
      value_type tmp;
      parse_element(ps, tmp, char_blacklist);
      if (ps.code > pec::trailing_character)
        return;
      *out++ = std::move(tmp);
    } while (ps.consume(','));
    ps.code = ps.at_end() ? pec::success : pec::trailing_character;
  }
}

template <class T>
enable_if_t<!is_pair<T>::value>
parse_element(string_parser_state& ps, T& x, const char*) {
  parse(ps, x);
}

template <class First, class Second, size_t N>
void parse_element(string_parser_state& ps, std::pair<First, Second>& kvp,
                   const char (&char_blacklist)[N]) {
  static_assert(N > 0, "empty array");
  // TODO: consider to guard the blacklist computation with
  //       `if constexpr (is_same_v<First, string>)` when switching to C++17.
  char key_blacklist[N + 1];
  if (N > 1)
    memcpy(key_blacklist, char_blacklist, N - 1);
  key_blacklist[N - 1] = '=';
  key_blacklist[N] = '\0';
  parse_element(ps, kvp.first, key_blacklist);
  if (ps.code > pec::trailing_character)
    return;
  if (!ps.consume('=')) {
    ps.code = pec::unexpected_character;
    return;
  }
  parse_element(ps, kvp.second, char_blacklist);
}

// -- convenience functions ----------------------------------------------------

CAF_CORE_EXPORT
error parse_result(const string_parser_state& ps, string_view input);

template <class T>
auto parse(string_view str, T& x) {
  string_parser_state ps{str.begin(), str.end()};
  parse(ps, x);
  return parse_result(ps, str);
}

template <class T, class Policy>
auto parse(string_view str, T& x, Policy policy) {
  string_parser_state ps{str.begin(), str.end()};
  parse(ps, x, policy);
  return parse_result(ps, str);
}

} // namespace caf::detail
