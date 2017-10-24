/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/config.hpp"

#define CAF_SUITE config_value
#include "caf/test/unit_test.hpp"

#include <string>

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/atom.hpp"
#include "caf/deep_to_string.hpp"
#include "caf/none.hpp"
#include "caf/variant.hpp"

using namespace std;
using namespace caf;

struct tostring_visitor : static_visitor<string> {
  template <class T>
  inline string operator()(const T& value) {
    return to_string(value);
  }
};

CAF_TEST(default_constructed) {
  config_value x;
  CAF_CHECK_EQUAL(holds_alternative<int64_t>(x), true);
  CAF_CHECK_EQUAL(get<int64_t>(x), 0);
}

CAF_TEST(list) {
  std::vector<config_value> xs;
  xs.emplace_back(int64_t{1});
  xs.emplace_back(atom("foo"));
  xs.emplace_back(string("bar"));
  config_value x{xs};
  CAF_CHECK_EQUAL(to_string(x), "[1, 'foo', \"bar\"]");
  auto ys = xs;
  xs.emplace_back(std::move(ys));
  x = xs;
  CAF_CHECK_EQUAL(to_string(x), "[1, 'foo', \"bar\", [1, 'foo', \"bar\"]]");
}

CAF_TEST(convert_to_list) {
  config_value x{int64_t{42}};
  CAF_CHECK_EQUAL(to_string(x), "42");
  x.convert_to_list();
  CAF_CHECK_EQUAL(to_string(x), "[42]");
  x.convert_to_list();
  CAF_CHECK_EQUAL(to_string(x), "[42]");
}

CAF_TEST(append) {
  config_value x{int64_t{1}};
  CAF_CHECK_EQUAL(to_string(x), "1");
  x.append(config_value{int64_t{2}});
  CAF_CHECK_EQUAL(to_string(x), "[1, 2]");
  x.append(config_value{atom("foo")});
  CAF_CHECK_EQUAL(to_string(x), "[1, 2, 'foo']");
}

CAF_TEST(maps) {
  std::map<std::string, config_value> xs;
  xs["num"] = int64_t{42};
  xs["atm"] = atom("hello");
  xs["str"] = string{"foobar"};
  xs["dur"] = timespan{100};
  config_value x{xs};
  CAF_CHECK_EQUAL(
    to_string(x),
    R"([("atm", 'hello'), ("dur", 100ns), ("num", 42), ("str", "foobar")])");
}
