// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/message.hpp"

#include "caf/test/approx.hpp"
#include "caf/test/test.hpp"

#include "caf/init_global_meta_objects.hpp"
#include "caf/log/test.hpp"
#include "caf/message_handler.hpp"
#include "caf/type_id.hpp"
#include "caf/type_id_list.hpp"

#include <map>
#include <numeric>
#include <string>
#include <vector>

using std::make_tuple;
using std::map;
using std::string;
using std::vector;

using namespace caf;
using namespace std::literals;

namespace {

struct s1;
struct s2;
struct s3;

} // namespace

CAF_BEGIN_TYPE_ID_BLOCK(message_test, caf::first_custom_type_id + 50)

  CAF_ADD_TYPE_ID(message_test, (s1))
  CAF_ADD_TYPE_ID(message_test, (s2))
  CAF_ADD_TYPE_ID(message_test, (s3))
  CAF_ADD_TYPE_ID(message_test, (std::tuple<int32_t, int32_t, int32_t>) )
  CAF_ADD_TYPE_ID(message_test, (std::vector<std::string>) )
  CAF_ADD_TYPE_ID(message_test, (std::vector<int>) )

CAF_END_TYPE_ID_BLOCK(message_test)

namespace {

struct s1 {
  int value[3] = {10, 20, 30};
};

template <class Inspector>
bool inspect(Inspector& f, s1& x) {
  return f.apply(x.value);
}

struct s2 {
  int value[4][2] = {{1, 10}, {2, 20}, {3, 30}, {4, 40}};
};

template <class Inspector>
bool inspect(Inspector& f, s2& x) {
  return f.apply(x.value);
}

struct s3 {
  std::array<int, 4> value;
  s3() {
    std::iota(value.begin(), value.end(), 1);
  }
};

template <class Inspector>
bool inspect(Inspector& f, s3& x) {
  return f.apply(x.value);
}

template <class... Ts>
std::string msg_as_string(Ts&&... xs) {
  return to_string(make_message(std::forward<Ts>(xs)...));
}

} // namespace

TEST("messages allow index-based access") {
  auto msg = make_message("abc"s, uint32_t{10}, 20.0);
  check_eq(msg.size(), 3u);
  check_eq(msg.types(), (make_type_id_list<std::string, uint32_t, double>()));
  check_eq(msg.get_as<std::string>(0), "abc");
  check_eq(msg.get_as<uint32_t>(1), 10u);
  check_eq(msg.get_as<double>(2), test::approx{20.0});
  check_eq(msg.cdata().get_reference_count(), 1u);
}

TEST("message detach their content on mutating access") {
  log::test::debug("Given to messages pointing to the same content.");
  auto msg1 = make_message("one"s, uint32_t{1});
  auto msg2 = msg1;
  check_eq(msg1.cdata().get_reference_count(), 2u);
  check_eq(msg1.cptr(), msg2.cptr());
  log::test::debug("When calling a non-const member function of message.");
  msg1.ptr();
  log::test::debug(
    "Then the messages point to separate contents but remain equal.");
  check_ne(msg1.cptr(), msg2.cptr());
  check_eq(msg1.cdata().get_reference_count(), 1u);
  check_eq(msg2.cdata().get_reference_count(), 1u);
  check((msg1.match_elements<std::string, uint32_t>()));
  check((msg2.match_elements<std::string, uint32_t>()));
  check_eq(msg1.get_as<std::string>(0), msg2.get_as<std::string>(0));
  check_eq(msg1.get_as<uint32_t>(1), msg2.get_as<uint32_t>(1));
}

TEST("messages can render custom types to strings") {
  s2 tmp;
  tmp.value[0][1] = 100;
  check_ne(to_string(make_message(s2{})), to_string(make_message(tmp)));
}

TEST("messages can render integers to strings") {
  using ivec = vector<int32_t>;
  using svec = vector<std::string>;
  using sset = std::set<std::string>;
  using std::string;
  using itup = std::tuple<int, int, int>;
  check_eq(make_message(ivec{}).types(), make_type_id_list<ivec>());
  check_eq(make_type_id_list<ivec>()[0], type_id_v<ivec>);
  check_eq(make_message(ivec{}).types()[0], type_id_v<ivec>);
  check_eq(make_message(1.0).types()[0], type_id_v<double>);
  check_eq(make_message(s1{}).types()[0], type_id_v<s1>);
  check_eq(make_message(s2{}).types()[0], type_id_v<s2>);
  check_eq(make_message(s3{}).types()[0], type_id_v<s3>);
  check_eq(make_message(svec{}).types()[0], type_id_v<svec>);
  check_eq(make_message(string{}).types()[0], type_id_v<string>);
  check_eq(make_message(sset{}).types()[0], type_id_v<sset>);
  check_eq(make_message(itup(1, 2, 3)).types()[0], type_id_v<itup>);
}

TEST("to_string converts messages to strings") {
  using svec = vector<string>;
  check_eq(msg_as_string(), "message()");
  check_eq(msg_as_string("hello", "world"), R"__(message("hello", "world"))__");
  check_eq(msg_as_string(svec{"one", "two", "three"}),
           R"__(message(["one", "two", "three"]))__");
  check_eq(msg_as_string(svec{"one", "two"}, "three", "four",
                         svec{"five", "six", "seven"}),
           R"__(message(["one", "two"], "three", "four", )__"
           R"__(["five", "six", "seven"]))__");
  auto teststr = R"__(message("this is a \"test\""))__"; // fails inline on MSVC
  check_eq(msg_as_string(R"__(this is a "test")__"), teststr);
  check_eq(msg_as_string(make_tuple(1, 2, 3), 4, 5),
           "message([1, 2, 3], 4, 5)");
  check_eq(msg_as_string(s1{}), "message([10, 20, 30])");
  s2 tmp;
  tmp.value[0][1] = 100;
  check_eq(msg_as_string(s2{}),
           "message([[1, 10], [2, 20], [3, 30], [4, 40]])");
  check_eq(msg_as_string(s3{}), "message([1, 2, 3, 4])");
}

TEST("messages are concatenable") {
  using std::make_tuple;
  check(message::concat(make_tuple(int16_t{1}), make_tuple(uint8_t{2}))
          .matches(int16_t{1}, uint8_t{2}));
  check(message::concat(make_message(int16_t{1}), make_message(uint8_t{2}))
          .matches(int16_t{1}, uint8_t{2}));
  check(message::concat(make_message(int16_t{1}), make_tuple(uint8_t{2}))
          .matches(int16_t{1}, uint8_t{2}));
  check(message::concat(make_tuple(int16_t{1}), make_message(uint8_t{2}))
          .matches(int16_t{1}, uint8_t{2}));
}

TEST("match_elements exposes element types") {
  auto msg = make_message(put_atom_v, "foo", int64_t{123});
  check((msg.match_element<put_atom>(0)));
  check((msg.match_element<string>(1)));
  check((msg.match_element<int64_t>(2)));
  check((msg.match_elements<put_atom, string, int64_t>()));
}

TEST("messages implicitly convert string-like things to strings") {
  auto is_hello_caf = [](const message& msg) {
    return msg.match_elements<string>() && msg.get_as<string>(0) == "hello CAF";
  };
  // From character array.
  char char_array[] = "hello CAF";
  check(is_hello_caf(make_message(char_array)));
  // From string_view.
  check(is_hello_caf(make_message("hello CAF"sv)));
  // From plain C-string literal.
  check(is_hello_caf(make_message("hello CAF")));
}

TEST_INIT() {
  init_global_meta_objects<id_block::message_test>();
}
