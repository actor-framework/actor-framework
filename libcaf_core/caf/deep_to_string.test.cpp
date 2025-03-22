// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/deep_to_string.hpp"

#include "caf/test/test.hpp"

#include <list>

using namespace caf;

TEST("timespans use the highest unit available when printing") {
  check_eq(to_string(config_value{timespan{0}}), "0s");
  check_eq(deep_to_string(timespan{0}), "0s");
  check_eq(deep_to_string(timespan{1}), "1ns");
  check_eq(deep_to_string(timespan{1'000}), "1us");
  check_eq(deep_to_string(timespan{1'000'000}), "1ms");
  check_eq(deep_to_string(timespan{1'000'000'000}), "1s");
  check_eq(deep_to_string(timespan{60'000'000'000}), "1min");
}

TEST("lists use square brackets") {
  int i32_array[] = {1, 2, 3, 4};
  using i32_array_type = std::array<int, 4>;
  check_eq(deep_to_string(std::list<int>({1, 2, 3, 4})), "[1, 2, 3, 4]");
  check_eq(deep_to_string(std::vector<int>({1, 2, 3, 4})), "[1, 2, 3, 4]");
  check_eq(deep_to_string(std::set<int>({1, 2, 3, 4})), "[1, 2, 3, 4]");
  check_eq(deep_to_string(i32_array_type({{1, 2, 3, 4}})), "[1, 2, 3, 4]");
  check_eq(deep_to_string(i32_array), "[1, 2, 3, 4]");
  bool bool_array[] = {false, true};
  using bool_array_type = std::array<bool, 2>;
  check_eq(deep_to_string(std::list<bool>({false, true})), "[false, true]");
  check_eq(deep_to_string(std::vector<bool>({false, true})), "[false, true]");
  check_eq(deep_to_string(std::set<bool>({false, true})), "[false, true]");
  check_eq(deep_to_string(bool_array_type({{false, true}})), "[false, true]");
  check_eq(deep_to_string(bool_array), "[false, true]");
}

TEST("pointers and optionals use dereference syntax") {
  auto i = 42;
  check_eq(deep_to_string(&i), "*42");
  check_eq(deep_to_string(static_cast<int*>(nullptr)), "null");
  check_eq(deep_to_string(std::optional<int>{}), "null");
  check_eq(deep_to_string(std::optional<int>{23}), "*23");
  check_eq(deep_to_string(std::vector<std::optional<int>>{{23}, {24}, {}}),
           "[*23, *24, null]");
  check_eq(deep_to_string(std::vector<int*>{&i, &i, nullptr}),
           "[*42, *42, null]");
}

TEST("buffers") {
  // Use `signed char` explicitly to make sure all compilers agree.
  std::vector<signed char> buf;
  check_eq(deep_to_string(buf), "[]");
  buf.push_back(-1);
  check_eq(deep_to_string(buf), "[-1]");
  buf.push_back(0);
  check_eq(deep_to_string(buf), "[-1, 0]");
  buf.push_back(127);
  check_eq(deep_to_string(buf), "[-1, 0, 127]");
  buf.push_back(10);
  check_eq(deep_to_string(buf), "[-1, 0, 127, 10]");
  buf.push_back(16);
  check_eq(deep_to_string(buf), "[-1, 0, 127, 10, 16]");
}
