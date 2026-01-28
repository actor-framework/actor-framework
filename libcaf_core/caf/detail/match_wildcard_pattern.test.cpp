// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/match_wildcard_pattern.hpp"

#include "caf/test/test.hpp"

using namespace caf;
using namespace std::literals;

TEST("an empty pattern matches nothing except for the empty string") {
  check(detail::match_wildcard_pattern(""sv, ""sv));
  check(!detail::match_wildcard_pattern("x"sv, ""sv));
  check(!detail::match_wildcard_pattern("file.txt"sv, ""sv));
}

TEST("empty input does not match non-empty pattern except for the asterisk") {
  check(!detail::match_wildcard_pattern(""sv, "x"sv));
  check(!detail::match_wildcard_pattern(""sv, "?"sv));
  check(detail::match_wildcard_pattern(""sv, "*"sv));
}

TEST("passing the input as pattern always matches") {
  check(detail::match_wildcard_pattern("file.txt"sv, "file.txt"sv));
  check(detail::match_wildcard_pattern("hello"sv, "hello"sv));
  check(!detail::match_wildcard_pattern("file.txt"sv, "file.dat"sv));
  check(!detail::match_wildcard_pattern("file.txt"sv, "other.txt"sv));
}

TEST("question mark matches exactly one character") {
  check(detail::match_wildcard_pattern("abc"sv, "?bc"sv));
  check(detail::match_wildcard_pattern("abc"sv, "a?c"sv));
  check(detail::match_wildcard_pattern("abc"sv, "ab?"sv));
}

TEST("asterisk matches zero or more characters") {
  check(detail::match_wildcard_pattern("file.txt"sv, "*.txt"sv));
  check(detail::match_wildcard_pattern("file.txt"sv, "file.*"sv));
  check(detail::match_wildcard_pattern("file.txt"sv, "f*i*e.*t"sv));
  check(detail::match_wildcard_pattern("file.txt"sv, "*"sv));
  check(detail::match_wildcard_pattern("file.txt"sv, "f*"sv));
  check(detail::match_wildcard_pattern("file.txt"sv, "*t"sv));
  check(detail::match_wildcard_pattern("file.txt"sv, "f*t"sv));
  check(detail::match_wildcard_pattern("file.txt"sv, "*.t*"sv));
  check(!detail::match_wildcard_pattern("other.txt"sv, "file*"sv));
  check(detail::match_wildcard_pattern("file.txt"sv, "*.*"sv));
  check(detail::match_wildcard_pattern("file.txt"sv, "f*.t*"sv));
  check(detail::match_wildcard_pattern("file.txt"sv, "*i*e*"sv));
}

TEST("repeated asterisks have the same effect as a single asterisk") {
  check(detail::match_wildcard_pattern("file.txt"sv, "*.txt"sv));
  check(detail::match_wildcard_pattern("file.txt"sv, "**.txt"sv));
  check(detail::match_wildcard_pattern("file.txt"sv, "***.txt"sv));
  check(detail::match_wildcard_pattern("file.txt"sv, "file*txt"sv));
  check(detail::match_wildcard_pattern("file.txt"sv, "file**txt"sv));
  check(detail::match_wildcard_pattern("file.txt"sv, "file***txt"sv));
  check(detail::match_wildcard_pattern("file.txt"sv, "file.*"sv));
  check(detail::match_wildcard_pattern("file.txt"sv, "file.**"sv));
  check(detail::match_wildcard_pattern("file.txt"sv, "file.***"sv));
}

TEST("asterisk and question mark combined") {
  check(detail::match_wildcard_pattern("file1.txt"sv, "file?.txt"sv));
  check(detail::match_wildcard_pattern("filea.txt"sv, "file?.txt"sv));
  check(detail::match_wildcard_pattern("file123.txt"sv, "file*.txt"sv));
  check(detail::match_wildcard_pattern("file456.txt"sv, "file*.txt"sv));
}

TEST("trailing asterisks") {
  check(detail::match_wildcard_pattern("file.txt"sv, "file*"sv));
  check(detail::match_wildcard_pattern("file.txt"sv, "file**"sv));
  check(detail::match_wildcard_pattern("file.txt"sv, "file***"sv));
}

TEST("mixing asterisk and question mark wildcards") {
  check(detail::match_wildcard_pattern("a"sv, "*"sv));
  check(detail::match_wildcard_pattern("a"sv, "?*"sv));
  check(detail::match_wildcard_pattern("a"sv, "*?"sv));
}
