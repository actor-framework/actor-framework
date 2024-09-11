// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/chrono.hpp"

#include "caf/test/outline.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/log/test.hpp"

using namespace caf;

using datetime = chrono::datetime;

TEST("a default constructed date and time is invalid") {
  datetime x;
  check(!x.valid());
}

SCENARIO("a date and time can be parsed from strings") {
  GIVEN("a valid date and time string with no UTC time zone information") {
    WHEN("parsing the string") {
      THEN("the date and time is valid and has an UTC offset is nullopt") {
        datetime x;
        check_eq(parse("2021-02-03T14:25:36", x), none);
        check(x.valid());
        check_eq(x.year, 2021);
        check_eq(x.month, 2);
        check_eq(x.day, 3);
        check_eq(x.hour, 14);
        check_eq(x.minute, 25);
        check_eq(x.second, 36);
        check_eq(x.nanosecond, 0);
        check_eq(x.utc_offset, std::nullopt);
      }
    }
  }
  GIVEN("a valid date and time string with Z suffix") {
    WHEN("parsing the string") {
      THEN("the date and time is valid and has an UTC offset of 0") {
        datetime x;
        check_eq(parse("2021-02-03T14:25:36Z", x), none);
        check(x.valid());
        check_eq(x.year, 2021);
        check_eq(x.month, 2);
        check_eq(x.day, 3);
        check_eq(x.hour, 14);
        check_eq(x.minute, 25);
        check_eq(x.second, 36);
        check_eq(x.nanosecond, 0);
        check_eq(x.utc_offset, 0);
      }
    }
  }
  GIVEN("a valid date and time string with a positive UTC offset") {
    WHEN("parsing the string") {
      THEN("the date and time is valid and has the specified UTC offset") {
        datetime x;
        check_eq(parse("2021-02-03T14:25:36+02:00", x), none);
        check(x.valid());
        check_eq(x.year, 2021);
        check_eq(x.month, 2);
        check_eq(x.day, 3);
        check_eq(x.hour, 14);
        check_eq(x.minute, 25);
        check_eq(x.second, 36);
        check_eq(x.nanosecond, 0);
        check_eq(x.utc_offset, 7200);
      }
    }
  }
  GIVEN("a valid date and time string with a negative UTC offset") {
    WHEN("parsing the string") {
      THEN("the date and time is valid and has the specified UTC offset") {
        datetime x;
        check_eq(parse("2021-02-03T14:25:36-01:30", x), none);
        check(x.valid());
        check_eq(x.year, 2021);
        check_eq(x.month, 2);
        check_eq(x.day, 3);
        check_eq(x.hour, 14);
        check_eq(x.minute, 25);
        check_eq(x.second, 36);
        check_eq(x.nanosecond, 0);
        check_eq(x.utc_offset, -5400);
      }
    }
  }
}

SCENARIO("a date and time with fractional seconds can be parsed from strings") {
  GIVEN("a valid date and time string with no UTC time zone information") {
    WHEN("parsing the string") {
      THEN("the date and time is valid and has an UTC offset is nullopt") {
        datetime x;
        check_eq(parse("2021-02-03T14:25:36.000", x), none);
        check(x.valid());
        check_eq(x.year, 2021);
        check_eq(x.month, 2);
        check_eq(x.day, 3);
        check_eq(x.hour, 14);
        check_eq(x.minute, 25);
        check_eq(x.second, 36);
        check_eq(x.nanosecond, 0);
        check_eq(x.utc_offset, std::nullopt);
      }
    }
  }
  GIVEN("a valid date and time string with Z suffix") {
    WHEN("parsing the string") {
      THEN("the date and time is valid and has an UTC offset of 0") {
        datetime x;
        check_eq(parse("2021-02-03T14:25:36.012Z", x), none);
        check(x.valid());
        check_eq(x.year, 2021);
        check_eq(x.month, 2);
        check_eq(x.day, 3);
        check_eq(x.hour, 14);
        check_eq(x.minute, 25);
        check_eq(x.second, 36);
        check_eq(x.nanosecond, 12'000'000);
        check_eq(x.utc_offset, 0);
      }
    }
  }
  GIVEN("a valid date and time string with a positive UTC offset") {
    WHEN("parsing the string") {
      THEN("the date and time is valid and has the specified UTC offset") {
        datetime x;
        check_eq(parse("2021-02-03T14:25:36.123+02:00", x), none);
        check(x.valid());
        check_eq(x.year, 2021);
        check_eq(x.month, 2);
        check_eq(x.day, 3);
        check_eq(x.hour, 14);
        check_eq(x.minute, 25);
        check_eq(x.second, 36);
        check_eq(x.nanosecond, 123'000'000);
        check_eq(x.utc_offset, 7200);
      }
    }
  }
  GIVEN("a valid date and time string with a negative UTC offset") {
    WHEN("parsing the string") {
      THEN("the date and time is valid and has the specified UTC offset") {
        datetime x;
        check_eq(parse("2021-02-03T14:25:36.999-01:30", x), none);
        check(x.valid());
        check_eq(x.year, 2021);
        check_eq(x.month, 2);
        check_eq(x.day, 3);
        check_eq(x.hour, 14);
        check_eq(x.minute, 25);
        check_eq(x.second, 36);
        check_eq(x.nanosecond, 999'000'000);
        check_eq(x.utc_offset, -5400);
      }
    }
  }
}

TEST("the parser refuses invalid date time values") {
  datetime x;
  auto invalid = make_error(pec::invalid_argument);
  check_eq(parse("2021-02-29T01:00:00", x), invalid); // Not a leap year.
  check_eq(parse("2021-00-10T01:00:00", x), invalid); // Month < 1.
  check_eq(parse("2021-13-10T01:00:00", x), invalid); // Month > 12.
  check_eq(parse("2021-01-00T01:00:00", x), invalid); // Day < 1.
  check_eq(parse("2021-01-32T01:00:00", x), invalid); // Day > 31.
  check_eq(parse("2021-01-01T24:00:00", x), invalid); // Hour > 23.
  check_eq(parse("2021-01-01T00:60:00", x), invalid); // Minute > 59.
  check_eq(parse("2021-01-01T00:00:60", x), invalid); // Second > 59.
}

SCENARIO("to_string produces valid input for parse") {
  GIVEN("a datetime without UTC offset") {
    WHEN("calling to_string() on it") {
      THEN("the result can be parsed again") {
        auto x = datetime{};
        x.year = 1999;
        x.month = 9;
        x.day = 9;
        x.hour = 9;
        x.minute = 9;
        x.second = 9;
        x.nanosecond = 9'000'000;
        auto x_str = to_string(x);
        check_eq(x_str, "1999-09-09T09:09:09.009");
        auto y = datetime{};
        check_eq(parse(x_str, y), none);
        check_eq(x, y);
        check_eq(x_str, to_string(y));
      }
    }
  }
  GIVEN("a datetime with a UTC offset of zero") {
    WHEN("calling to_string() on it") {
      THEN("the result can be parsed again") {
        auto x = datetime{};
        x.year = 2010;
        x.month = 10;
        x.day = 10;
        x.hour = 10;
        x.minute = 10;
        x.second = 10;
        x.nanosecond = 99'000'000;
        x.utc_offset = 0;
        auto x_str = to_string(x);
        check_eq(x_str, "2010-10-10T10:10:10.099Z");
        auto y = datetime{};
        check_eq(parse(x_str, y), none);
        check_eq(x, y);
        check_eq(x_str, to_string(y));
      }
    }
  }
  GIVEN("a datetime with positive UTC offset") {
    WHEN("calling to_string() on it") {
      THEN("the result can be parsed again") {
        auto x = datetime{};
        x.year = 2211;
        x.month = 11;
        x.day = 11;
        x.hour = 11;
        x.minute = 11;
        x.second = 11;
        x.nanosecond = 999'000'000;
        x.utc_offset = 5400;
        auto x_str = to_string(x);
        check_eq(x_str, "2211-11-11T11:11:11.999+01:30");
        auto y = datetime{};
        check_eq(parse(x_str, y), none);
        check_eq(x, y);
        check_eq(x_str, to_string(y));
      }
    }
  }
  GIVEN("a datetime with negative UTC offset") {
    WHEN("calling to_string() on it") {
      THEN("the result can be parsed again") {
        auto x = datetime{};
        x.year = 1122;
        x.month = 12;
        x.day = 12;
        x.hour = 12;
        x.minute = 12;
        x.second = 12;
        x.nanosecond = 999'000'000;
        x.utc_offset = -5400;
        auto x_str = to_string(x);
        check_eq(x_str, "1122-12-12T12:12:12.999-01:30");
        auto y = datetime{};
        check_eq(parse(x_str, y), none);
        check_eq(x, y);
        check_eq(x_str, to_string(y));
      }
    }
  }
}

TEST("the fractional component may have 1-9 digits") {
  auto from_string = [](std::string_view str) {
    return datetime::from_string(str);
  };
  // 1 digit.
  check_eq(from_string("2021-02-03T14:25:36.1"),
           from_string("2021-02-03T14:25:36.10"));
  check_eq(from_string("2021-02-03T14:25:36.1"),
           from_string("2021-02-03T14:25:36.100"));
  check_eq(from_string("2021-02-03T14:25:36.1"),
           from_string("2021-02-03T14:25:36.1000"));
  check_eq(from_string("2021-02-03T14:25:36.1"),
           from_string("2021-02-03T14:25:36.10000"));
  check_eq(from_string("2021-02-03T14:25:36.1"),
           from_string("2021-02-03T14:25:36.100000"));
  check_eq(from_string("2021-02-03T14:25:36.1"),
           from_string("2021-02-03T14:25:36.1000000"));
  check_eq(from_string("2021-02-03T14:25:36.1"),
           from_string("2021-02-03T14:25:36.10000000"));
  check_eq(from_string("2021-02-03T14:25:36.1"),
           from_string("2021-02-03T14:25:36.100000000"));
  // 2 digits.
  check_eq(from_string("2021-02-03T14:25:36.12"),
           from_string("2021-02-03T14:25:36.120"));
  check_eq(from_string("2021-02-03T14:25:36.12"),
           from_string("2021-02-03T14:25:36.1200"));
  check_eq(from_string("2021-02-03T14:25:36.12"),
           from_string("2021-02-03T14:25:36.12000"));
  check_eq(from_string("2021-02-03T14:25:36.12"),
           from_string("2021-02-03T14:25:36.120000"));
  check_eq(from_string("2021-02-03T14:25:36.12"),
           from_string("2021-02-03T14:25:36.1200000"));
  check_eq(from_string("2021-02-03T14:25:36.12"),
           from_string("2021-02-03T14:25:36.12000000"));
  check_eq(from_string("2021-02-03T14:25:36.12"),
           from_string("2021-02-03T14:25:36.120000000"));
  // 3 digits.
  check_eq(from_string("2021-02-03T14:25:36.123"),
           from_string("2021-02-03T14:25:36.1230"));
  check_eq(from_string("2021-02-03T14:25:36.123"),
           from_string("2021-02-03T14:25:36.12300"));
  check_eq(from_string("2021-02-03T14:25:36.123"),
           from_string("2021-02-03T14:25:36.123000"));
  check_eq(from_string("2021-02-03T14:25:36.123"),
           from_string("2021-02-03T14:25:36.1230000"));
  check_eq(from_string("2021-02-03T14:25:36.123"),
           from_string("2021-02-03T14:25:36.12300000"));
  check_eq(from_string("2021-02-03T14:25:36.123"),
           from_string("2021-02-03T14:25:36.123000000"));
  // 4 digits.
  check_eq(from_string("2021-02-03T14:25:36.1234"),
           from_string("2021-02-03T14:25:36.12340"));
  check_eq(from_string("2021-02-03T14:25:36.1234"),
           from_string("2021-02-03T14:25:36.123400"));
  check_eq(from_string("2021-02-03T14:25:36.1234"),
           from_string("2021-02-03T14:25:36.1234000"));
  check_eq(from_string("2021-02-03T14:25:36.1234"),
           from_string("2021-02-03T14:25:36.12340000"));
  check_eq(from_string("2021-02-03T14:25:36.1234"),
           from_string("2021-02-03T14:25:36.123400000"));
  // 5 digits.
  check_eq(from_string("2021-02-03T14:25:36.12345"),
           from_string("2021-02-03T14:25:36.123450"));
  check_eq(from_string("2021-02-03T14:25:36.12345"),
           from_string("2021-02-03T14:25:36.1234500"));
  check_eq(from_string("2021-02-03T14:25:36.12345"),
           from_string("2021-02-03T14:25:36.12345000"));
  check_eq(from_string("2021-02-03T14:25:36.12345"),
           from_string("2021-02-03T14:25:36.123450000"));
  // 6 digits.
  check_eq(from_string("2021-02-03T14:25:36.123456"),
           from_string("2021-02-03T14:25:36.1234560"));
  check_eq(from_string("2021-02-03T14:25:36.123456"),
           from_string("2021-02-03T14:25:36.12345600"));
  check_eq(from_string("2021-02-03T14:25:36.123456"),
           from_string("2021-02-03T14:25:36.123456000"));
  // 7 digits.
  check_eq(from_string("2021-02-03T14:25:36.1234567"),
           from_string("2021-02-03T14:25:36.12345670"));
  check_eq(from_string("2021-02-03T14:25:36.1234567"),
           from_string("2021-02-03T14:25:36.123456700"));
  // 8 digits.
  check_eq(from_string("2021-02-03T14:25:36.12345678"),
           from_string("2021-02-03T14:25:36.123456780"));
  // 9 digits
  if (auto x = from_string("2021-02-03T14:25:36.123456789");
      check(x.has_value())) {
    check_eq(x->nanosecond, 123456789);
  }
}

TEST("chrono::to_string generates valid input for datetime::parse") {
  // We know neither the local timezone nor what the system clock returns, so we
  // can only check that the string is valid by parsing it again.
  SECTION("std::chrono time point") {
    auto str = chrono::to_string(std::chrono::system_clock::now());
    log::test::debug("str = {}", str);
    check(datetime::from_string(str).has_value());
  }
  SECTION("caf::timestamp") {
    auto str = chrono::to_string(make_timestamp());
    log::test::debug("str = {}", str);
    check(datetime::from_string(str).has_value());
  }
}

TEST("chrono::to_string and chrono::print generate the same string") {
  auto ts = std::chrono::system_clock::now();
  auto str1 = chrono::to_string(ts);
  auto str2 = std::string{};
  chrono::print(str2, ts);
  check_eq(str1, str2);
}

OUTLINE("two timestamps with the same time point are equal") {
  GIVEN("the timestamp <lhs>") {
    auto lhs = datetime::from_string(block_parameters<std::string>());
    require(lhs.has_value());
    WHEN("comparing it to <rhs>") {
      auto rhs = datetime::from_string(block_parameters<std::string>());
      require(rhs.has_value());
      THEN("they should compare equal") {
        check_eq(*lhs, *rhs);
      }
    }
  }
  EXAMPLES = R"__(
    | lhs                       | rhs                       |
    | 2024-05-16T21:00:00+09:00 | 2024-05-16T12:00:00Z      |
    | 2024-05-16T08:00:00+09:00 | 2024-05-15T23:00:00Z      |
    | 2024-05-16T07:00:00-05:00 | 2024-05-16T12:00:00Z      |
    | 2024-05-16T20:00:00-05:00 | 2024-05-17T01:00:00Z      |
    | 2024-05-16T12:00:00Z      | 2024-05-16T21:00:00+09:00 |
    | 2024-05-15T23:00:00Z      | 2024-05-16T08:00:00+09:00 |
    | 2024-05-16T12:00:00Z      | 2024-05-16T07:00:00-05:00 |
    | 2024-05-17T01:00:00Z      | 2024-05-16T20:00:00-05:00 |
    | 2024-05-16T12:00:00+00:30 | 2024-05-16T11:30:00Z      |
    | 2024-05-16T00:15:00+00:30 | 2024-05-15T23:45:00Z      |
    | 2024-05-16T12:45:00-00:30 | 2024-05-16T13:15:00Z      |
    | 2024-05-16T23:45:00-00:30 | 2024-05-17T00:15:00Z      |
    | 2024-05-16T11:30:00Z      | 2024-05-16T12:00:00+00:30 |
    | 2024-05-15T23:45:00Z      | 2024-05-16T00:15:00+00:30 |
    | 2024-05-16T13:15:00Z      | 2024-05-16T12:45:00-00:30 |
    | 2024-05-17T00:15:00Z      | 2024-05-16T23:45:00-00:30 |
  )__";
}

OUTLINE("force_utc coverts a datetime object to UTC") {
  GIVEN("the timestamp <timestamp>") {
    auto maybe_ts = datetime::from_string(block_parameters<std::string>());
    require(maybe_ts.has_value());
    auto ts = *maybe_ts;
    WHEN("calling force_utc() on it") {
      ts.force_utc();
      THEN("the result is <expected>") {
        check_eq(ts.to_string(), block_parameters<std::string>());
      }
    }
  }
  EXAMPLES = R"__(
    | timestamp                 | expected             |
    | 2024-05-16T12:00:00       | 2024-05-16T12:00:00Z |
    | 2024-05-16T21:00:00+09:00 | 2024-05-16T12:00:00Z |
    | 2024-05-16T08:00:00+09:00 | 2024-05-15T23:00:00Z |
    | 2024-05-16T07:00:00-05:00 | 2024-05-16T12:00:00Z |
    | 2024-05-16T20:00:00-05:00 | 2024-05-17T01:00:00Z |
    | 2024-05-16T12:00:00+00:30 | 2024-05-16T11:30:00Z |
    | 2024-05-16T00:15:00+00:30 | 2024-05-15T23:45:00Z |
    | 2024-05-16T12:45:00-00:30 | 2024-05-16T13:15:00Z |
    | 2024-05-16T23:45:00-00:30 | 2024-05-17T00:15:00Z |
  )__";
}

TEST("to_string prints fractional digits according to the precision") {
  using chrono::fixed;
  using s = std::chrono::seconds;
  using ms = std::chrono::milliseconds;
  using us = std::chrono::microseconds;
  using ns = std::chrono::nanoseconds;
  SECTION("no fractional digits") {
    auto dt = datetime{2021, 2, 3, 14, 30, 0, 0, 0};
    check_eq(dt.to_string<s>(), "2021-02-03T14:30:00Z");
    check_eq(dt.to_string<ms>(), "2021-02-03T14:30:00Z");
    check_eq(dt.to_string<us>(), "2021-02-03T14:30:00Z");
    check_eq(dt.to_string<ns>(), "2021-02-03T14:30:00Z");
    check_eq(dt.to_string<s, fixed>(), "2021-02-03T14:30:00Z");
    check_eq(dt.to_string<ms, fixed>(), "2021-02-03T14:30:00.000Z");
    check_eq(dt.to_string<us, fixed>(), "2021-02-03T14:30:00.000000Z");
    check_eq(dt.to_string<ns, fixed>(), "2021-02-03T14:30:00.000000000Z");
  }
  SECTION("up to three fractional digits") {
    auto dt = datetime{2021, 2, 3, 14, 30, 0, 123000000, 0};
    check_eq(dt.to_string<s>(), "2021-02-03T14:30:00Z");
    check_eq(dt.to_string<ms>(), "2021-02-03T14:30:00.123Z");
    check_eq(dt.to_string<us>(), "2021-02-03T14:30:00.123Z");
    check_eq(dt.to_string<ns>(), "2021-02-03T14:30:00.123Z");
    check_eq(dt.to_string<s, fixed>(), "2021-02-03T14:30:00Z");
    check_eq(dt.to_string<ms, fixed>(), "2021-02-03T14:30:00.123Z");
    check_eq(dt.to_string<us, fixed>(), "2021-02-03T14:30:00.123000Z");
    check_eq(dt.to_string<ns, fixed>(), "2021-02-03T14:30:00.123000000Z");
  }
  SECTION("up to six fractional digits") {
    auto dt = datetime{2021, 2, 3, 14, 30, 0, 123456000, 0};
    check_eq(dt.to_string<s>(), "2021-02-03T14:30:00Z");
    check_eq(dt.to_string<ms>(), "2021-02-03T14:30:00.123Z");
    check_eq(dt.to_string<us>(), "2021-02-03T14:30:00.123456Z");
    check_eq(dt.to_string<ns>(), "2021-02-03T14:30:00.123456Z");
    check_eq(dt.to_string<s, fixed>(), "2021-02-03T14:30:00Z");
    check_eq(dt.to_string<ms, fixed>(), "2021-02-03T14:30:00.123Z");
    check_eq(dt.to_string<us, fixed>(), "2021-02-03T14:30:00.123456Z");
    check_eq(dt.to_string<ns, fixed>(), "2021-02-03T14:30:00.123456000Z");
  }
  SECTION("up to nine fractional digits") {
    auto dt = datetime{2021, 2, 3, 14, 30, 0, 123456789, 0};
    check_eq(dt.to_string<s>(), "2021-02-03T14:30:00Z");
    check_eq(dt.to_string<ms>(), "2021-02-03T14:30:00.123Z");
    check_eq(dt.to_string<us>(), "2021-02-03T14:30:00.123456Z");
    check_eq(dt.to_string<ns>(), "2021-02-03T14:30:00.123456789Z");
    check_eq(dt.to_string<s, fixed>(), "2021-02-03T14:30:00Z");
    check_eq(dt.to_string<ms, fixed>(), "2021-02-03T14:30:00.123Z");
    check_eq(dt.to_string<us, fixed>(), "2021-02-03T14:30:00.123456Z");
    check_eq(dt.to_string<ns, fixed>(), "2021-02-03T14:30:00.123456789Z");
  }
  SECTION("free function") {
    auto dt = datetime{2021, 2, 3, 14, 30, 0, 0, 0};
    check_eq(to_string(dt), dt.to_string());
  }
}
