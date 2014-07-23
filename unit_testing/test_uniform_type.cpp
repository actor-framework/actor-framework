#include <map>
#include <set>
#include <memory>
#include <cctype>
#include <atomic>
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <iostream>
#include <iterator>
#include <algorithm>
#include <stdexcept>

#include "test.hpp"

#include "caf/all.hpp"
#include "caf/io/all.hpp"

using std::cout;
using std::endl;

namespace {

struct foo {
  int value;
  explicit foo(int val = 0) : value(val) {}

};

inline bool operator==(const foo& lhs, const foo& rhs) {
  return lhs.value == rhs.value;
}

enum class test_enum {
  test_value1,
  test_value2
};

struct test_struct {
  test_enum test_value;
};

} // namespace <anonymous>

using namespace caf;

bool check_types(const std::set<std::string>& expected) {
  // holds the type names we see at runtime
  std::set<std::string> found;
  // fetch all available type names
  auto types = uniform_type_info::instances();
  for (auto tinfo : types) {
    found.insert(tinfo->name());
  }
  // compare the two sets
  CAF_CHECK_EQUAL(expected.size(), found.size());
  bool expected_equals_found = false;
  if (expected.size() == found.size()
    && std::equal(found.begin(), found.end(), expected.begin())) {
    CAF_CHECKPOINT();
    return true;
  }
  CAF_CHECK(false);
  if (!expected_equals_found) {
    std::string(41, ' ');
    std::ostringstream oss(std::string(41, ' '));
    oss.seekp(0);
    oss << "found (" << found.size() << ")";
    oss.seekp(22);
    oss << "expected (" << expected.size() << ")";
    std::string lhs;
    std::string rhs;
    CAF_PRINT(oss.str());
    CAF_PRINT(std::string(41, '-'));
    auto fi = found.begin();
    auto fe = found.end();
    auto ei = expected.begin();
    auto ee = expected.end();
    while (fi != fe || ei != ee) {
      if (fi != fe)
        lhs = *fi++;
      else
        lhs.clear();
      if (ei != ee)
        rhs = *ei++;
      else
        rhs.clear();
      lhs.resize(20, ' ');
      CAF_PRINT(lhs << "| " << rhs);
    }
  }
  return false;
}

template <class T>
T& append(T& storage) {
  return storage;
}

template <class T, typename U, class... Us>
T& append(T& storage, U&& u, Us&&... us) {
  storage.insert(std::forward<U>(u));
  return append(storage, std::forward<Us>(us)...);
}

int main() {
  CAF_TEST(test_uniform_type);
  auto announce1 = announce<foo>(&foo::value);
  auto announce2 = announce<foo>(&foo::value);
  auto announce3 = announce<foo>(&foo::value);
  auto announce4 = announce<foo>(&foo::value);
  CAF_CHECK(announce1 == announce2);
  CAF_CHECK(announce1 == announce3);
  CAF_CHECK(announce1 == announce4);
  CAF_CHECK_EQUAL(announce1->name(), "$::foo");
  {
    auto uti = uniform_typeid<atom_value>();
    CAF_CHECK(uti != nullptr);
    CAF_CHECK_EQUAL(uti->name(), "@atom");
  }
  // these types (and only those) are present if
  // the uniform_type_info implementation is correct
  std::set<std::string> expected = {
    // local types
    "$::foo", // <anonymous namespace>::foo
    // primitive types
    "bool",         "@i8",   "@i16",
    "@i32",         "@i64", // signed integer names
    "@u8",        "@u16",  "@u32",
    "@u64",                    // unsigned integer names
    "@str",         "@u16str", "@u32str",  // strings
    "float",        "double",  "@ldouble", // floating points
    // default announced types
    "@unit",        // unit_t
    "@actor",       // actor
    "@addr",        // actor_addr
    "@atom",        // atom_value
    "@channel",       // channel
    "@charbuf",       // vector<char>
    "@down",        // down_msg
    "@duration",      // duration
    "@exit",        // exit_msg
    "@group",       // group
    "@group_down",    // group_down_msg
    "@message",       // message
    "@message_id",    // message_id
    "@node",        // node_id
    "@strmap",      // map<string,string>
    "@timeout",       // timeout_msg
    "@sync_exited",     // sync_exited_msg
    "@sync_timeout",    // sync_timeout_msg
    "@strvec"       // vector<string>
  };
  if (check_types(expected)) {
    // causes the middleman to create its singleton
    io::middleman::instance();
    // ok, check whether middleman announces its types correctly
    check_types(append(expected,
                       "caf::io::accept_handle",
                       "caf::io::acceptor_closed_msg",
                       "caf::io::connection_handle",
                       "caf::io::connection_closed_msg",
                       "caf::io::new_connection_msg",
                       "caf::io::new_data_msg"));
  }
  // check whether enums can be announced as members
  announce<test_enum>();
  announce<test_struct>(&test_struct::test_value);
  return CAF_TEST_RESULT();
}
