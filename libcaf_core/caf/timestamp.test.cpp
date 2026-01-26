// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/timestamp.hpp"

#include "caf/test/test.hpp"

#include "caf/chrono.hpp"
#include "caf/error_code.hpp"
#include "caf/expected.hpp"
#include "caf/pec.hpp"
#include "caf/string_algorithms.hpp"

#include <chrono>
#include <string>

using namespace caf;
using namespace std::literals;

TEST("make_timestamp returns current system time") {
  auto before = std::chrono::system_clock::now();
  auto ts = make_timestamp();
  auto after = std::chrono::system_clock::now();
  check_ge(ts, timestamp{before.time_since_epoch()});
  check_le(ts, timestamp{after.time_since_epoch()});
}

TEST("timestamp_to_string converts timestamp to a numerical representation") {
  auto known_time = std::chrono::system_clock::time_point{1577836800s};
  auto ts = timestamp{known_time.time_since_epoch()};
  check_eq(timestamp_to_string(ts),
           std::to_string(ts.time_since_epoch().count()));
}

TEST("timestamp_from_string parses valid ISO 8601 timestamps") {
  SECTION("basic format without timezone") {
    if (auto result = timestamp_from_string("2021-02-03T14:25:36");
        check_has_value(result)) {
      check_gt(result->time_since_epoch().count(), 0LL);
    }
  }
  SECTION("Z suffix (UTC)") {
    if (auto result = timestamp_from_string("2021-02-03T14:25:36Z");
        check_has_value(result)) {
      check_gt(result->time_since_epoch().count(), 0LL);
    }
  }
  SECTION("fractional seconds") {
    if (auto result = timestamp_from_string("2021-02-03T14:25:36.123");
        check_has_value(result)) {
      check_gt(result->time_since_epoch().count(), 0LL);
    }
  }
  SECTION("fractional seconds with Z suffix (UTC)") {
    if (auto result = timestamp_from_string("2021-02-03T14:25:36.123456Z");
        check_has_value(result)) {
      check_gt(result->time_since_epoch().count(), 0LL);
    }
  }
  SECTION("positive UTC offset") {
    if (auto result = timestamp_from_string("2021-02-03T14:25:36+02:00");
        check_has_value(result)) {
      check_gt(result->time_since_epoch().count(), 0LL);
    }
  }
  SECTION("negative UTC offset") {
    if (auto result = timestamp_from_string("2021-02-03T14:25:36-01:30");
        check_has_value(result)) {
      check_gt(result->time_since_epoch().count(), 0LL);
    }
  }
  SECTION("fractional seconds and UTC offset") {
    if (auto result = timestamp_from_string("2021-02-03T14:25:36.123+02:00");
        check_has_value(result)) {
      check_gt(result->time_since_epoch().count(), 0LL);
    }
  }
}

TEST("timestamp_from_string returns errors for invalid inputs") {
  SECTION("empty string") {
    check_eq(timestamp_from_string(""), error_code{pec::unexpected_eof});
  }
  SECTION("invalid format") {
    check_eq(timestamp_from_string("not a timestamp"),
             error_code{pec::unexpected_character});
  }
  SECTION("missing date parts") {
    check_eq(timestamp_from_string("2021-02"), error_code{pec::unexpected_eof});
    check_eq(timestamp_from_string("2021-02-03"),
             error_code{pec::unexpected_eof});
  }
  SECTION("missing time separator") {
    check_eq(timestamp_from_string("2021-02-03 14:25:36"),
             error_code{pec::unexpected_character});
  }
  SECTION("invalid date") {
    check_eq(timestamp_from_string("2021-13-01T14:25:36"),
             error_code{pec::invalid_argument});
    check_eq(timestamp_from_string("2021-02-30T14:25:36"),
             error_code{pec::invalid_argument});
  }
  SECTION("invalid time") {
    check_eq(timestamp_from_string("2021-02-03T25:00:00"),
             error_code{pec::invalid_argument});
    check_eq(timestamp_from_string("2021-02-03T14:60:00"),
             error_code{pec::invalid_argument});
    check_eq(timestamp_from_string("2021-02-03T14:25:60"),
             error_code{pec::invalid_argument});
  }
}

TEST("append_timestamp_to_string appends timestamp to string") {
  auto known_time = std::chrono::system_clock::time_point{1577836800s};
  auto ts = timestamp{known_time.time_since_epoch()};
  auto expected_suffix = timestamp_to_string(ts);
  SECTION("appending to non-empty string") {
    std::string str = "prefix:";
    append_timestamp_to_string(str, ts);
    check(starts_with(str, "prefix:"));
    check_eq(str, "prefix:" + expected_suffix);
  }
  SECTION("appending to empty string") {
    std::string empty;
    append_timestamp_to_string(empty, ts);
    check_eq(empty, expected_suffix);
  }
  SECTION("appending multiple times") {
    std::string multi = "start:";
    append_timestamp_to_string(multi, ts);
    append_timestamp_to_string(multi, ts);
    check_eq(multi, "start:" + expected_suffix + expected_suffix);
  }
}

TEST("timestamp round-trip conversion preserves timestamp value") {
  auto val = make_timestamp();
  auto iso_str = chrono::to_string(val);
  if (auto parsed = timestamp_from_string(iso_str); check_has_value(parsed)) {
    check_eq(val, *parsed);
  }
}
