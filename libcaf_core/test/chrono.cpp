// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE chrono

#include "caf/chrono.hpp"

#include "caf/test/bdd_dsl.hpp"

using namespace caf;

using datetime = chrono::datetime;

TEST_CASE("a default constructed date and time is invalid") {
  datetime x;
  CHECK(!x.valid());
}

SCENARIO("a date and time can be parsed from strings") {
  GIVEN("a valid date and time string with no UTC time zone information") {
    WHEN("parsing the string") {
      THEN("the date and time is valid and has an UTC offset is nullopt") {
        datetime x;
        CHECK_EQ(parse("2021-02-03T14:25:36", x), none);
        CHECK(x.valid());
        CHECK_EQ(x.year, 2021);
        CHECK_EQ(x.month, 2);
        CHECK_EQ(x.day, 3);
        CHECK_EQ(x.hour, 14);
        CHECK_EQ(x.minute, 25);
        CHECK_EQ(x.second, 36);
        CHECK_EQ(x.nanosecond, 0);
        CHECK_EQ(x.utc_offset, std::nullopt);
      }
    }
  }
  GIVEN("a valid date and time string with Z suffix") {
    WHEN("parsing the string") {
      THEN("the date and time is valid and has an UTC offset of 0") {
        datetime x;
        CHECK_EQ(parse("2021-02-03T14:25:36Z", x), none);
        CHECK(x.valid());
        CHECK_EQ(x.year, 2021);
        CHECK_EQ(x.month, 2);
        CHECK_EQ(x.day, 3);
        CHECK_EQ(x.hour, 14);
        CHECK_EQ(x.minute, 25);
        CHECK_EQ(x.second, 36);
        CHECK_EQ(x.nanosecond, 0);
        CHECK_EQ(x.utc_offset, 0);
      }
    }
  }
  GIVEN("a valid date and time string with a positive UTC offset") {
    WHEN("parsing the string") {
      THEN("the date and time is valid and has the specified UTC offset") {
        datetime x;
        CHECK_EQ(parse("2021-02-03T14:25:36+02:00", x), none);
        CHECK(x.valid());
        CHECK_EQ(x.year, 2021);
        CHECK_EQ(x.month, 2);
        CHECK_EQ(x.day, 3);
        CHECK_EQ(x.hour, 14);
        CHECK_EQ(x.minute, 25);
        CHECK_EQ(x.second, 36);
        CHECK_EQ(x.nanosecond, 0);
        CHECK_EQ(x.utc_offset, 7200);
      }
    }
  }
  GIVEN("a valid date and time string with a negative UTC offset") {
    WHEN("parsing the string") {
      THEN("the date and time is valid and has the specified UTC offset") {
        datetime x;
        CHECK_EQ(parse("2021-02-03T14:25:36-01:30", x), none);
        CHECK(x.valid());
        CHECK_EQ(x.year, 2021);
        CHECK_EQ(x.month, 2);
        CHECK_EQ(x.day, 3);
        CHECK_EQ(x.hour, 14);
        CHECK_EQ(x.minute, 25);
        CHECK_EQ(x.second, 36);
        CHECK_EQ(x.nanosecond, 0);
        CHECK_EQ(x.utc_offset, -5400);
      }
    }
  }
}

SCENARIO("a date and time with fractional seconds can be parsed from strings") {
  GIVEN("a valid date and time string with no UTC time zone information") {
    WHEN("parsing the string") {
      THEN("the date and time is valid and has an UTC offset is nullopt") {
        datetime x;
        CHECK_EQ(parse("2021-02-03T14:25:36.000", x), none);
        CHECK(x.valid());
        CHECK_EQ(x.year, 2021);
        CHECK_EQ(x.month, 2);
        CHECK_EQ(x.day, 3);
        CHECK_EQ(x.hour, 14);
        CHECK_EQ(x.minute, 25);
        CHECK_EQ(x.second, 36);
        CHECK_EQ(x.nanosecond, 0);
        CHECK_EQ(x.utc_offset, std::nullopt);
      }
    }
  }
  GIVEN("a valid date and time string with Z suffix") {
    WHEN("parsing the string") {
      THEN("the date and time is valid and has an UTC offset of 0") {
        datetime x;
        CHECK_EQ(parse("2021-02-03T14:25:36.012Z", x), none);
        CHECK(x.valid());
        CHECK_EQ(x.year, 2021);
        CHECK_EQ(x.month, 2);
        CHECK_EQ(x.day, 3);
        CHECK_EQ(x.hour, 14);
        CHECK_EQ(x.minute, 25);
        CHECK_EQ(x.second, 36);
        CHECK_EQ(x.nanosecond, 12'000'000);
        CHECK_EQ(x.utc_offset, 0);
      }
    }
  }
  GIVEN("a valid date and time string with a positive UTC offset") {
    WHEN("parsing the string") {
      THEN("the date and time is valid and has the specified UTC offset") {
        datetime x;
        CHECK_EQ(parse("2021-02-03T14:25:36.123+02:00", x), none);
        CHECK(x.valid());
        CHECK_EQ(x.year, 2021);
        CHECK_EQ(x.month, 2);
        CHECK_EQ(x.day, 3);
        CHECK_EQ(x.hour, 14);
        CHECK_EQ(x.minute, 25);
        CHECK_EQ(x.second, 36);
        CHECK_EQ(x.nanosecond, 123'000'000);
        CHECK_EQ(x.utc_offset, 7200);
      }
    }
  }
  GIVEN("a valid date and time string with a negative UTC offset") {
    WHEN("parsing the string") {
      THEN("the date and time is valid and has the specified UTC offset") {
        datetime x;
        CHECK_EQ(parse("2021-02-03T14:25:36.999-01:30", x), none);
        CHECK(x.valid());
        CHECK_EQ(x.year, 2021);
        CHECK_EQ(x.month, 2);
        CHECK_EQ(x.day, 3);
        CHECK_EQ(x.hour, 14);
        CHECK_EQ(x.minute, 25);
        CHECK_EQ(x.second, 36);
        CHECK_EQ(x.nanosecond, 999'000'000);
        CHECK_EQ(x.utc_offset, -5400);
      }
    }
  }
}

TEST_CASE("the parser refuses invalid date time values") {
  datetime x;
  auto invalid = make_error(pec::invalid_argument);
  CHECK_EQ(parse("2021-02-29T01:00:00", x), invalid); // Not a leap year.
  CHECK_EQ(parse("2021-00-10T01:00:00", x), invalid); // Month < 1.
  CHECK_EQ(parse("2021-13-10T01:00:00", x), invalid); // Month > 12.
  CHECK_EQ(parse("2021-01-00T01:00:00", x), invalid); // Day < 1.
  CHECK_EQ(parse("2021-01-32T01:00:00", x), invalid); // Day > 31.
  CHECK_EQ(parse("2021-01-01T24:00:00", x), invalid); // Hour > 23.
  CHECK_EQ(parse("2021-01-01T00:60:00", x), invalid); // Minute > 59.
  CHECK_EQ(parse("2021-01-01T00:00:60", x), invalid); // Second > 59.
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
        CHECK_EQ(x_str, "1999-09-09T09:09:09.009");
        auto y = datetime{};
        CHECK_EQ(parse(x_str, y), none);
        CHECK_EQ(x, y);
        CHECK_EQ(x_str, to_string(y));
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
        CHECK_EQ(x_str, "2010-10-10T10:10:10.099Z");
        auto y = datetime{};
        CHECK_EQ(parse(x_str, y), none);
        CHECK_EQ(x, y);
        CHECK_EQ(x_str, to_string(y));
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
        CHECK_EQ(x_str, "2211-11-11T11:11:11.999+01:30");
        auto y = datetime{};
        CHECK_EQ(parse(x_str, y), none);
        CHECK_EQ(x, y);
        CHECK_EQ(x_str, to_string(y));
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
        CHECK_EQ(x_str, "1122-12-12T12:12:12.999-01:30");
        auto y = datetime{};
        CHECK_EQ(parse(x_str, y), none);
        CHECK_EQ(x, y);
        CHECK_EQ(x_str, to_string(y));
      }
    }
  }
}

TEST_CASE("the fractional component may have 1-9 digits") {
  auto from_string = [](std::string_view str) {
    return datetime::from_string(str);
  };
  // 1 digit.
  CHECK_EQ(from_string("2021-02-03T14:25:36.1"),
           from_string("2021-02-03T14:25:36.10"));
  CHECK_EQ(from_string("2021-02-03T14:25:36.1"),
           from_string("2021-02-03T14:25:36.100"));
  CHECK_EQ(from_string("2021-02-03T14:25:36.1"),
           from_string("2021-02-03T14:25:36.1000"));
  CHECK_EQ(from_string("2021-02-03T14:25:36.1"),
           from_string("2021-02-03T14:25:36.10000"));
  CHECK_EQ(from_string("2021-02-03T14:25:36.1"),
           from_string("2021-02-03T14:25:36.100000"));
  CHECK_EQ(from_string("2021-02-03T14:25:36.1"),
           from_string("2021-02-03T14:25:36.1000000"));
  CHECK_EQ(from_string("2021-02-03T14:25:36.1"),
           from_string("2021-02-03T14:25:36.10000000"));
  CHECK_EQ(from_string("2021-02-03T14:25:36.1"),
           from_string("2021-02-03T14:25:36.100000000"));
  // 2 digits.
  CHECK_EQ(from_string("2021-02-03T14:25:36.12"),
           from_string("2021-02-03T14:25:36.120"));
  CHECK_EQ(from_string("2021-02-03T14:25:36.12"),
           from_string("2021-02-03T14:25:36.1200"));
  CHECK_EQ(from_string("2021-02-03T14:25:36.12"),
           from_string("2021-02-03T14:25:36.12000"));
  CHECK_EQ(from_string("2021-02-03T14:25:36.12"),
           from_string("2021-02-03T14:25:36.120000"));
  CHECK_EQ(from_string("2021-02-03T14:25:36.12"),
           from_string("2021-02-03T14:25:36.1200000"));
  CHECK_EQ(from_string("2021-02-03T14:25:36.12"),
           from_string("2021-02-03T14:25:36.12000000"));
  CHECK_EQ(from_string("2021-02-03T14:25:36.12"),
           from_string("2021-02-03T14:25:36.120000000"));
  // 3 digits.
  CHECK_EQ(from_string("2021-02-03T14:25:36.123"),
           from_string("2021-02-03T14:25:36.1230"));
  CHECK_EQ(from_string("2021-02-03T14:25:36.123"),
           from_string("2021-02-03T14:25:36.12300"));
  CHECK_EQ(from_string("2021-02-03T14:25:36.123"),
           from_string("2021-02-03T14:25:36.123000"));
  CHECK_EQ(from_string("2021-02-03T14:25:36.123"),
           from_string("2021-02-03T14:25:36.1230000"));
  CHECK_EQ(from_string("2021-02-03T14:25:36.123"),
           from_string("2021-02-03T14:25:36.12300000"));
  CHECK_EQ(from_string("2021-02-03T14:25:36.123"),
           from_string("2021-02-03T14:25:36.123000000"));
  // 4 digits.
  CHECK_EQ(from_string("2021-02-03T14:25:36.1234"),
           from_string("2021-02-03T14:25:36.12340"));
  CHECK_EQ(from_string("2021-02-03T14:25:36.1234"),
           from_string("2021-02-03T14:25:36.123400"));
  CHECK_EQ(from_string("2021-02-03T14:25:36.1234"),
           from_string("2021-02-03T14:25:36.1234000"));
  CHECK_EQ(from_string("2021-02-03T14:25:36.1234"),
           from_string("2021-02-03T14:25:36.12340000"));
  CHECK_EQ(from_string("2021-02-03T14:25:36.1234"),
           from_string("2021-02-03T14:25:36.123400000"));
  // 5 digits.
  CHECK_EQ(from_string("2021-02-03T14:25:36.12345"),
           from_string("2021-02-03T14:25:36.123450"));
  CHECK_EQ(from_string("2021-02-03T14:25:36.12345"),
           from_string("2021-02-03T14:25:36.1234500"));
  CHECK_EQ(from_string("2021-02-03T14:25:36.12345"),
           from_string("2021-02-03T14:25:36.12345000"));
  CHECK_EQ(from_string("2021-02-03T14:25:36.12345"),
           from_string("2021-02-03T14:25:36.123450000"));
  // 6 digits.
  CHECK_EQ(from_string("2021-02-03T14:25:36.123456"),
           from_string("2021-02-03T14:25:36.1234560"));
  CHECK_EQ(from_string("2021-02-03T14:25:36.123456"),
           from_string("2021-02-03T14:25:36.12345600"));
  CHECK_EQ(from_string("2021-02-03T14:25:36.123456"),
           from_string("2021-02-03T14:25:36.123456000"));
  // 7 digits.
  CHECK_EQ(from_string("2021-02-03T14:25:36.1234567"),
           from_string("2021-02-03T14:25:36.12345670"));
  CHECK_EQ(from_string("2021-02-03T14:25:36.1234567"),
           from_string("2021-02-03T14:25:36.123456700"));
  // 8 digits.
  CHECK_EQ(from_string("2021-02-03T14:25:36.12345678"),
           from_string("2021-02-03T14:25:36.123456780"));
  // 9 digits
  if (auto x = from_string("2021-02-03T14:25:36.123456789"); CHECK(x)) {
    CHECK_EQ(x->nanosecond, 123456789);
  }
}

TEST_CASE("chrono::to_string generates valid input for datetime::parse") {
  // We know neither the local timezone nor what the system clock returns, so we
  // can only check that the string is valid by parsing it again.
  { // std::chrono time point
    auto str = chrono::to_string(std::chrono::system_clock::now());
    CHECK(datetime::from_string(str));
  }
  { // caf::timestamp
    auto str = chrono::to_string(make_timestamp());
    CHECK(datetime::from_string(str));
  }
}

TEST_CASE("chrono::to_string and chrono::print generate the same string") {
  auto ts = std::chrono::system_clock::now();
  auto str1 = chrono::to_string(ts);
  auto str2 = std::string{};
  chrono::print(str2, ts);
  CHECK_EQ(str1, str2);
}

TEST_CASE("two timestamps with the same time point are equal") {
  auto from_string = [](std::string_view str) {
    return datetime::from_string(str);
  };
  CHECK_EQ(from_string("2021-02-03T14:25:36Z"),
           from_string("2021-02-03T14:25:36+00:00"));
  CHECK_EQ(from_string("2021-02-03T14:25:36Z"),
           from_string("2021-02-03T15:25:36+01:00"));
  CHECK_EQ(from_string("2021-02-03T14:25:36Z"),
           from_string("2021-02-03T13:25:36-01:00"));
  CHECK_EQ(from_string("2021-02-03T15:25:36+01:00"),
           from_string("2021-02-03T13:25:36-01:00"));
}
