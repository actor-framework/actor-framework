/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
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

#define CAF_SUITE ini_parser
#include "caf/test/unit_test.hpp"
#include <iostream>
#include <sstream>

#include "caf/all.hpp"

#include "caf/detail/parse_ini.hpp"

using namespace caf;

namespace {

template <class T>
bool value_is(const config_value& cv, const T& what) {
  const T* ptr = get<T>(&cv);
  return ptr != nullptr && *ptr == what;
}

constexpr const char* case1 = R"__(
[scheduler]
policy="work-sharing"
max-threads=2
; the middleman
[middleman]
automatic-connections=true

[nexus]
host="127.0.0.1"
port=4242

[cash]
greeting="Hi there, this is \"CASH!\"\n ~\\~ use at your own risk ~\\~"
)__";

constexpr const char* case2 = R"__(
[test]
foo=-0xff
bar=034
baz=-0.23
buzz=1E-34

)__";

} // namespace <anonymous>

CAF_TEST(simple_ini) {
  std::map<std::string, config_value> values;
  auto f = [&](std::string key, config_value value) {
    values.emplace(std::move(key), std::move(value));
  };
  std::stringstream ss;
  std::stringstream err;
  ss << case1;
  detail::parse_ini(ss, err, f);
  CAF_CHECK(values.count("nexus.port") > 0);
  CAF_CHECK(value_is(values["nexus.port"], int64_t{4242}));
  CAF_CHECK(value_is(values["nexus.host"], std::string{"127.0.0.1"}));
  CAF_CHECK(value_is(values["scheduler.policy"], std::string{"work-sharing"}));
  CAF_CHECK(value_is(values["scheduler.max-threads"], int64_t{2}));
  CAF_CHECK(value_is(values["middleman.automatic-connections"], bool{true}));
  CAF_CHECK(values.count("cash.greeting") > 0);
  CAF_CHECK(value_is(values["cash.greeting"],std::string{
              "Hi there, this is \"CASH!\"\n ~\\~ use at your own risk ~\\~"
            }));
}

CAF_TEST(numbers) {
  std::map<std::string, config_value> values;
  auto f = [&](std::string key, config_value value) {
    values.emplace(std::move(key), std::move(value));
  };
  std::stringstream ss;
  std::stringstream err;
  ss << case2;
  detail::parse_ini(ss, err, f);
  CAF_CHECK(value_is(values["test.foo"], int64_t{-0xff}));
  CAF_CHECK(value_is(values["test.bar"], int64_t{034}));
  CAF_CHECK(value_is(values["test.baz"], double{-0.23}));
  CAF_CHECK(value_is(values["test.buzz"], double{1E-34}));
}
