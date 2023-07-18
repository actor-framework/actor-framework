// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/chrono.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/squashed_int.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/error.hpp"
#include "caf/fwd.hpp"
#include "caf/message.hpp"
#include "caf/none.hpp"
#include "caf/parser_state.hpp"
#include "caf/unit.hpp"

#include <cstdint>
#include <cstring>
#include <iterator>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

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
  std::string_view str;
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

// When parsing regular integers, a "071" is 57 because the parser reads it as
// an octal number. This wrapper forces the parser to ignore leading zeros
// instead and always read numbers as decimals.
template <class Integer>
struct zero_padded_integer {
  Integer val = 0;
};

template <class Integer>
void parse(string_parser_state& ps, zero_padded_integer<Integer>& x) {
  x.val = 0;
  ps.skip_whitespaces();
  if (ps.at_end()) {
    // Let the actual parser implementation set an appropriate error code.
    parse(ps, x.val);
  } else {
    // Skip all leading zeros and then dispatch to the matching parser.
    auto c = ps.current();
    auto j = ps.i + 1;
    while (c == '0' && j != ps.e && isdigit(*j)) {
      c = ps.next();
      ++j;
    }
    parse(ps, x.val);
  }
}

// -- floating point types -----------------------------------------------------

CAF_CORE_EXPORT void parse(string_parser_state& ps, float& x);

CAF_CORE_EXPORT void parse(string_parser_state& ps, double& x);

CAF_CORE_EXPORT void parse(string_parser_state& ps, long double& x);

// -- CAF types ----------------------------------------------------------------

CAF_CORE_EXPORT void parse(string_parser_state&, ipv4_address&);

CAF_CORE_EXPORT void parse(string_parser_state&, ipv4_subnet&);

CAF_CORE_EXPORT void parse(string_parser_state&, ipv4_endpoint&);

CAF_CORE_EXPORT void parse(string_parser_state&, ipv6_address&);

CAF_CORE_EXPORT void parse(string_parser_state&, ipv6_subnet&);

CAF_CORE_EXPORT void parse(string_parser_state&, ipv6_endpoint&);

CAF_CORE_EXPORT void parse(string_parser_state&, uri&);

CAF_CORE_EXPORT void parse(string_parser_state&, config_value&);

CAF_CORE_EXPORT void parse(string_parser_state&, std::vector<config_value>&);

CAF_CORE_EXPORT void parse(string_parser_state&, dictionary<config_value>&);

// -- variadic utility ---------------------------------------------------------

template <class... Ts>
bool parse_sequence(string_parser_state& ps, Ts&&... xs) {
  auto parse_one = [&ps](auto& x) {
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
  chrono::datetime dt;
  parse(ps, dt);
  if (ps.code != pec::success)
    return;
  x = dt.to_local_time<Duration>();
}

// -- convenience functions ----------------------------------------------------

CAF_CORE_EXPORT
error parse_result(const string_parser_state& ps, std::string_view input);

template <class T>
auto parse(std::string_view str, T& x) {
  string_parser_state ps{str.begin(), str.end()};
  parse(ps, x);
  return parse_result(ps, str);
}

template <class T, class Policy>
auto parse(std::string_view str, T& x, Policy policy) {
  string_parser_state ps{str.begin(), str.end()};
  parse(ps, x, policy);
  return parse_result(ps, str);
}

} // namespace caf::detail
