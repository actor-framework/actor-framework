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

#include "caf/detail/type_nr.hpp"

using std::cout;
using std::endl;

namespace {

struct foo {
  int value;
  explicit foo(int val = 0) : value(val) {
    // nop
  }
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

template <class T>
std::string tostr(T value, size_t fieldwidth = 2) {
  std::ostringstream oss;
  oss.width(fieldwidth);
  oss << value;
  return oss.str();
}

bool check_types(const std::map<std::string, uint16_t>& expected) {
  // holds the type names we see at runtime
  std::map<std::string, uint16_t> found;
  // fetch all available type names
  auto types = uniform_type_info::instances();
  for (auto tinfo : types) {
    found.insert(std::make_pair(tinfo->name(), tinfo->type_nr()));
  }
  // compare the two maps
  if (expected == found) {
    CAF_CHECKPOINT();
    return true;
  }
  CAF_CHECK(false);
  using const_iterator = std::map<std::string, uint16_t>::const_iterator;
  using std::setw;
  using std::left;
  std::ostringstream oss;
  oss << left << setw(20) << ("found (" + tostr(found.size(), 1) + ")")
      << "  |  expected (" << expected.size() << ")";
  CAF_PRINT(oss.str());
  oss.seekp(0);
  oss << std::setfill('-') << setw(22) << "" << "|" << setw(22) << "";
  CAF_PRINT(oss.str());
  auto fi = found.cbegin();
  auto fe = found.cend();
  auto ei = expected.cbegin();
  auto ee = expected.cend();
  std::string dummy(20, ' ');
  auto out = [&](const_iterator& i, const_iterator last) -> std::string {
    if (i == last) {
      return dummy;
    }
    std::ostringstream oss;
    oss << left << setw(16) << i->first << "[" << tostr(i->second) << "]";
    ++i;
    return oss.str();
  };
  while (fi != fe || ei != ee) {
    CAF_PRINT(out(fi, fe) << "  |  " << out(ei, ee));
  }
  return false;
}

template <class T>
T& append(T& storage) {
  return storage;
}

template <class T, typename U, class... Us>
T& append(T& storage, U&& u, Us&&... us) {
  storage.insert(std::make_pair(std::forward<U>(u), uint16_t{0}));
  return append(storage, std::forward<Us>(us)...);
}

template <class T>
constexpr uint16_t tnr() {
  return detail::type_nr<T>::value;
}

int main() {
  CAF_TEST(test_uniform_type);
  auto announce1 = announce<foo>("foo", &foo::value);
  auto announce2 = announce<foo>("foo", &foo::value);
  auto announce3 = announce<foo>("foo", &foo::value);
  auto announce4 = announce<foo>("foo", &foo::value);
  CAF_CHECK(announce1 == announce2);
  CAF_CHECK(announce1 == announce3);
  CAF_CHECK(announce1 == announce4);
  CAF_CHECK_EQUAL(announce1->name(), "foo");
  {
    auto uti = uniform_typeid<atom_value>();
    CAF_CHECK(uti != nullptr);
    CAF_CHECK_EQUAL(uti->name(), "@atom");
  }
  using detail::type_nr;
  // these types (and only those) are present if
  // the uniform_type_info implementation is correct
  std::map<std::string, uint16_t> expected{
    // local types
    {"foo", 0},
    // primitive types
    {"bool", tnr<bool>()},
    // signed integer names
    {"@i8", tnr<int8_t>()},
    {"@i16", tnr<int16_t>()},
    {"@i32", tnr<int32_t>()},
    {"@i64", tnr<int64_t>()},
    // unsigned integer names
    {"@u8", tnr<uint8_t>()},
    {"@u16", tnr<uint16_t>()},
    {"@u32", tnr<uint32_t>()},
    {"@u64", tnr<uint64_t>()},
    // strings
    {"@str", tnr<std::string>()},
    {"@u16str", tnr<std::u16string>()},
    {"@u32str", tnr<std::u32string>()},
    // floating points
    {"float", tnr<float>()},
    {"double", tnr<double>()},
    {"@ldouble", tnr<long double>()},
    // default announced types
    {"@unit", tnr<unit_t>()},
    {"@actor", tnr<actor>()},
    {"@addr", tnr<actor_addr>()},
    {"@atom", tnr<atom_value>()},
    {"@channel", tnr<channel>()},
    {"@charbuf", tnr<std::vector<char>>()},
    {"@down", tnr<down_msg>()},
    {"@duration", tnr<duration>()},
    {"@exit", tnr<exit_msg>()},
    {"@group", tnr<group>()},
    {"@group_down", tnr<group_down_msg>()},
    {"@message", tnr<message>()},
    {"@message_id", tnr<message_id>()},
    {"@node", tnr<node_id>()},
    {"@strmap", tnr<std::map<std::string,std::string>>()},
    {"@timeout", tnr<timeout_msg>()},
    {"@sync_exited", tnr<sync_exited_msg>()},
    {"@sync_timeout", tnr<sync_timeout_msg>()},
    {"@strvec", tnr<std::vector<std::string>>()},
    {"@strset", tnr<std::set<std::string>>()}
  };
  CAF_CHECKPOINT();
  if (check_types(expected)) {
    CAF_CHECKPOINT();
    // causes the middleman to create its singleton
    io::middleman::instance();
    CAF_CHECKPOINT();
    // ok, check whether middleman announces its types correctly
    check_types(append(expected,
                       "caf::io::accept_handle",
                       "caf::io::acceptor_closed_msg",
                       "caf::io::connection_handle",
                       "caf::io::connection_closed_msg",
                       "caf::io::new_connection_msg",
                       "caf::io::new_data_msg"));
    CAF_CHECKPOINT();
  }
  // check whether enums can be announced as members
  announce<test_enum>("test_enum");
  announce<test_struct>("test_struct", &test_struct::test_value);
  CAF_CHECKPOINT();
  check_types(append(expected, "test_enum", "test_struct"));
  shutdown();
  return CAF_TEST_RESULT();
}
