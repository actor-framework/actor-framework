// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE message

#include "caf/message.hpp"

#include "core-test.hpp"

#include <map>
#include <numeric>
#include <string>
#include <vector>

#include "caf/init_global_meta_objects.hpp"
#include "caf/message_handler.hpp"
#include "caf/type_id.hpp"
#include "caf/type_id_list.hpp"

using std::make_tuple;
using std::map;
using std::string;
using std::vector;

using namespace caf;

using namespace std::literals::string_literals;

namespace {

template <class... Ts>
std::string msg_as_string(Ts&&... xs) {
  return to_string(make_message(std::forward<Ts>(xs)...));
}

} // namespace

CAF_TEST(messages allow index - based access) {
  auto msg = make_message("abc", uint32_t{10}, 20.0);
  CHECK_EQ(msg.size(), 3u);
  CHECK_EQ(msg.types(), (make_type_id_list<std::string, uint32_t, double>()));
  CHECK_EQ(msg.get_as<std::string>(0), "abc");
  CHECK_EQ(msg.get_as<uint32_t>(1), 10u);
  CHECK_EQ(msg.get_as<double>(2), 20.0);
  CHECK_EQ(msg.cdata().get_reference_count(), 1u);
}

CAF_TEST(message detach their content on mutating access) {
  MESSAGE("Given to messages pointing to the same content.");
  auto msg1 = make_message("one", uint32_t{1});
  auto msg2 = msg1;
  CHECK_EQ(msg1.cdata().get_reference_count(), 2u);
  CHECK_EQ(msg1.cptr(), msg2.cptr());
  MESSAGE("When calling a non-const member function of message.");
  msg1.ptr();
  MESSAGE("Then the messages point to separate contents but remain equal.");
  CHECK_NE(msg1.cptr(), msg2.cptr());
  CHECK_EQ(msg1.cdata().get_reference_count(), 1u);
  CHECK_EQ(msg2.cdata().get_reference_count(), 1u);
  CHECK((msg1.match_elements<std::string, uint32_t>()));
  CHECK((msg2.match_elements<std::string, uint32_t>()));
  CHECK_EQ(msg1.get_as<std::string>(0), msg2.get_as<std::string>(0));
  CHECK_EQ(msg1.get_as<uint32_t>(1), msg2.get_as<uint32_t>(1));
}

CAF_TEST(compare_custom_types) {
  s2 tmp;
  tmp.value[0][1] = 100;
  CHECK_NE(to_string(make_message(s2{})), to_string(make_message(tmp)));
}

CAF_TEST(integers_to_string) {
  using ivec = vector<int32_t>;
  using svec = vector<std::string>;
  using sset = std::set<std::string>;
  using std::string;
  using itup = std::tuple<int, int, int>;
  CHECK_EQ(make_message(ivec{}).types(), make_type_id_list<ivec>());
  CHECK_EQ(make_type_id_list<ivec>()[0], type_id_v<ivec>);
  CHECK_EQ(make_message(ivec{}).types()[0], type_id_v<ivec>);
  CHECK_EQ(make_message(1.0).types()[0], type_id_v<double>);
  CHECK_EQ(make_message(s1{}).types()[0], type_id_v<s1>);
  CHECK_EQ(make_message(s2{}).types()[0], type_id_v<s2>);
  CHECK_EQ(make_message(s3{}).types()[0], type_id_v<s3>);
  CHECK_EQ(make_message(svec{}).types()[0], type_id_v<svec>);
  CHECK_EQ(make_message(string{}).types()[0], type_id_v<string>);
  CHECK_EQ(make_message(sset{}).types()[0], type_id_v<sset>);
  CHECK_EQ(make_message(itup(1, 2, 3)).types()[0], type_id_v<itup>);
}

CAF_TEST(to_string converts messages to strings) {
  using svec = vector<string>;
  CHECK_EQ(msg_as_string(), "message()");
  CHECK_EQ(msg_as_string("hello", "world"), R"__(message("hello", "world"))__");
  CHECK_EQ(msg_as_string(svec{"one", "two", "three"}),
           R"__(message(["one", "two", "three"]))__");
  CHECK_EQ(msg_as_string(svec{"one", "two"}, "three", "four",
                         svec{"five", "six", "seven"}),
           R"__(message(["one", "two"], "three", "four", )__"
           R"__(["five", "six", "seven"]))__");
  auto teststr = R"__(message("this is a \"test\""))__"; // fails inline on MSVC
  CHECK_EQ(msg_as_string(R"__(this is a "test")__"), teststr);
  CHECK_EQ(msg_as_string(make_tuple(1, 2, 3), 4, 5),
           "message([1, 2, 3], 4, 5)");
  CHECK_EQ(msg_as_string(s1{}), "message([10, 20, 30])");
  s2 tmp;
  tmp.value[0][1] = 100;
  CHECK_EQ(msg_as_string(s2{}),
           "message([[1, 10], [2, 20], [3, 30], [4, 40]])");
  CHECK_EQ(msg_as_string(s3{}), "message([1, 2, 3, 4])");
}

CAF_TEST(match_elements exposes element types) {
  auto msg = make_message(put_atom_v, "foo", int64_t{123});
  CHECK((msg.match_element<put_atom>(0)));
  CHECK((msg.match_element<string>(1)));
  CHECK((msg.match_element<int64_t>(2)));
  CHECK((msg.match_elements<put_atom, string, int64_t>()));
}

CAF_TEST(messages are concatenable) {
  using std::make_tuple;
  CHECK(message::concat(make_tuple(int16_t{1}), make_tuple(uint8_t{2}))
          .matches(int16_t{1}, uint8_t{2}));
  CHECK(message::concat(make_message(int16_t{1}), make_message(uint8_t{2}))
          .matches(int16_t{1}, uint8_t{2}));
  CHECK(message::concat(make_message(int16_t{1}), make_tuple(uint8_t{2}))
          .matches(int16_t{1}, uint8_t{2}));
  CHECK(message::concat(make_tuple(int16_t{1}), make_message(uint8_t{2}))
          .matches(int16_t{1}, uint8_t{2}));
}
