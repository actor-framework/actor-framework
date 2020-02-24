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

#define CAF_SUITE message

#include "caf/message.hpp"

#include "caf/test/dsl.hpp"

#include <map>
#include <numeric>
#include <string>
#include <vector>

#include "caf/init_global_meta_objects.hpp"
#include "caf/message_handler.hpp"
#include "caf/type_id.hpp"
#include "caf/type_id_list.hpp"

namespace {

struct s1;
struct s2;
struct s3;

} // namespace

CAF_BEGIN_TYPE_ID_BLOCK(message_tests, first_custom_type_id)

  CAF_ADD_TYPE_ID(message_tests, s1)

  CAF_ADD_TYPE_ID(message_tests, s2)

  CAF_ADD_TYPE_ID(message_tests, s3)

  CAF_ADD_TYPE_ID(message_tests, std::vector<int>)

  CAF_ADD_TYPE_ID(message_tests, std::vector<std::string>)

  CAF_ADD_TYPE_ID(message_tests, std::map<int CAF_PP_COMMA int>)

  CAF_ADD_TYPE_ID(message_tests,
                  std::tuple<int CAF_PP_COMMA int CAF_PP_COMMA int>)

  CAF_ADD_TYPE_ID(
    message_tests,
    std::tuple<std::string CAF_PP_COMMA int32_t CAF_PP_COMMA uint32_t>)

CAF_END_TYPE_ID_BLOCK(message_tests)

using std::map;
using std::string;
using std::vector;
using std::make_tuple;

using namespace caf;

using namespace std::literals::string_literals;

namespace {

struct s1 {
  int value[3] = {10, 20, 30};
};

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, s1& x) {
  return f(x.value);
}

struct s2 {
  int value[4][2] = {{1, 10}, {2, 20}, {3, 30}, {4, 40}};
};

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, s2& x) {
  return f(x.value);
}

struct s3 {
  std::array<int, 4> value;
  s3() {
    std::iota(value.begin(), value.end(), 1);
  }
};

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, s3& x) {
  return f(x.value);
}

template <class... Ts>
std::string msg_as_string(Ts&&... xs) {
  return to_string(make_message(std::forward<Ts>(xs)...));
}

struct config : actor_system_config {
  config() {
    init_global_meta_objects<message_tests_type_ids>();
  }
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(message_tests, test_coordinator_fixture<config>)

CAF_TEST(messages allow index-based access) {
  auto msg = make_message("abc", uint32_t{10}, 20.0);
  CAF_CHECK_EQUAL(msg.size(), 3u);
  CAF_CHECK_EQUAL(msg.types(),
                  (make_type_id_list<std::string, uint32_t, double>()));
  CAF_CHECK_EQUAL(msg.get_as<std::string>(0), "abc");
  CAF_CHECK_EQUAL(msg.get_as<uint32_t>(1), 10u);
  CAF_CHECK_EQUAL(msg.get_as<double>(2), 20.0);
  CAF_CHECK_EQUAL(msg.cdata().get_reference_count(), 1u);
}

CAF_TEST(compare_custom_types) {
  s2 tmp;
  tmp.value[0][1] = 100;
  CAF_CHECK_NOT_EQUAL(to_string(make_message(s2{})),
                      to_string(make_message(tmp)));
}

CAF_TEST(empty_to_string) {
  message msg;
  CAF_CHECK_EQUAL(to_string(msg), "<empty-message>");
}

CAF_TEST(integers_to_string) {
  using ivec = vector<int32_t>;
  CAF_CHECK_EQUAL(msg_as_string(1, 2, 3), "(1, 2, 3)");
  CAF_CHECK_EQUAL(msg_as_string(ivec{1, 2, 3}), "([1, 2, 3])");
  CAF_CHECK_EQUAL(msg_as_string(ivec{1, 2}, 3, 4, ivec{5, 6, 7}),
                  "([1, 2], 3, 4, [5, 6, 7])");
  auto msg = make_message(ivec{1, 2, 3});
  CAF_MESSAGE("s1: " << type_id_v<s1>);
  CAF_MESSAGE("ivec: " << type_id_v<ivec>);
  CAF_MESSAGE("msg.types: " << msg.types());
  CAF_MESSAGE("types #1: " << make_type_id_list<s1>());
  CAF_MESSAGE("types #2: " << make_type_id_list<ivec>());
  CAF_CHECK_EQUAL(msg.get_as<ivec>(0), ivec({1, 2, 3}));
}

CAF_TEST(strings_to_string) {
  using svec = vector<string>;
  auto msg1 = make_message("one", "two", "three");
  CAF_CHECK_EQUAL(to_string(msg1), R"__(("one", "two", "three"))__");
  auto msg2 = make_message(svec{"one", "two", "three"});
  CAF_CHECK_EQUAL(to_string(msg2), R"__((["one", "two", "three"]))__");
  auto msg3 = make_message(svec{"one", "two"}, "three", "four",
                           svec{"five", "six", "seven"});
  CAF_CHECK(to_string(msg3) ==
          R"__((["one", "two"], "three", "four", ["five", "six", "seven"]))__");
  auto msg4 = make_message(R"(this is a "test")");
  CAF_CHECK_EQUAL(to_string(msg4), "(\"this is a \\\"test\\\"\")");
}

CAF_TEST(maps_to_string) {
  map<int, int> m1{{1, 10}, {2, 20}, {3, 30}};
  auto msg1 = make_message(move(m1));
  CAF_CHECK_EQUAL(to_string(msg1), "({1 = 10, 2 = 20, 3 = 30})");
}

CAF_TEST(tuples_to_string) {
  auto msg1 = make_message(make_tuple(1, 2, 3), 4, 5);
  CAF_CHECK_EQUAL(to_string(msg1), "((1, 2, 3), 4, 5)");
  auto msg2 = make_message(make_tuple("one"s, int32_t{2}, uint32_t{3}), 4, true);
  CAF_CHECK_EQUAL(to_string(msg2), "((\"one\", 2, 3), 4, true)");
}

CAF_TEST(arrays_to_string) {
  CAF_CHECK_EQUAL(msg_as_string(s1{}), "([10, 20, 30])");
  auto msg2 = make_message(s2{});
  s2 tmp;
  tmp.value[0][1] = 100;
  CAF_CHECK_EQUAL(to_string(msg2), "([[1, 10], [2, 20], [3, 30], [4, 40]])");
  CAF_CHECK_EQUAL(msg_as_string(s3{}), "([1, 2, 3, 4])");
}

CAF_TEST(match_elements exposes element types) {
  auto msg = make_message(put_atom_v, "foo", int64_t{123});
  CAF_CHECK((msg.match_element<put_atom>(0)));
  CAF_CHECK((msg.match_element<string>(1)));
  CAF_CHECK((msg.match_element<int64_t>(2)));
  CAF_CHECK((msg.match_elements<put_atom, string, int64_t>()));
}

CAF_TEST_FIXTURE_SCOPE_END()
