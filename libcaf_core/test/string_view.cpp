// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE string_view

#include "caf/string_view.hpp"

#include "core-test.hpp"

using namespace caf;
using namespace caf::literals;

CAF_TEST(default construction) {
  string_view x;
  string_view y;
  CHECK(x.empty());
  CHECK_EQ(x.size(), 0u);
  CHECK_EQ(x.data(), nullptr);
  CHECK_EQ(y, y);
}

CAF_TEST(cstring conversion) {
  auto x = "abc"_sv;
  CHECK_EQ(x.size(), 3u);
  CHECK_EQ(x[0], 'a');
  CHECK_EQ(x[1], 'b');
  CHECK_EQ(x[2], 'c');
  CHECK_EQ(x, "abc");
  x = "def";
  CHECK_NE(x, "abc");
  CHECK_EQ(x, "def");
}

CAF_TEST(string conversion) {
  std::string x = "abc";
  string_view y;
  y = x;
  CHECK_EQ(x, y);
  auto f = [&](string_view z) { CHECK_EQ(x, z); };
  f(x);
}

CAF_TEST(substrings) {
  auto without_prefix = [](string_view str, size_t n) {
    str.remove_prefix(n);
    return str;
  };
  auto without_suffix = [](string_view str, size_t n) {
    str.remove_suffix(n);
    return str;
  };
  auto x = "abcdefghi"_sv;
  CHECK_EQ(without_prefix(x, 3), "defghi");
  CHECK_EQ(without_suffix(x, 3), "abcdef");
  CHECK_EQ(x.substr(3, 3), "def");
  CHECK_EQ(without_prefix(x, 9), "");
  CHECK_EQ(without_suffix(x, 9), "");
  CHECK_EQ(x.substr(9), "");
  CHECK_EQ(x.substr(0, 0), "");
}

CAF_TEST(compare) {
  // testees
  auto x = "abc"_sv;
  auto y = "bcd"_sv;
  auto z = "cde"_sv;
  // x.compare full strings
  CHECK(x.compare("abc") == 0);
  CHECK(x.compare(y) < 0);
  CHECK(x.compare(z) < 0);
  // y.compare full strings
  CHECK(y.compare(x) > 0);
  CHECK(y.compare("bcd") == 0);
  CHECK(y.compare(z) < 0);
  // z.compare full strings
  CHECK(z.compare(x) > 0);
  CHECK(z.compare(y) > 0);
  CHECK(z.compare("cde") == 0);
  // x.compare substrings
  CHECK(x.compare(0, 3, "abc") == 0);
  CHECK(x.compare(1, 2, y, 0, 2) == 0);
  CHECK(x.compare(2, 1, z, 0, 1) == 0);
  // make sure substrings aren't equal
  CHECK("a/"_sv != "a/b"_sv);
  CHECK(z.compare("cdef"_sv) < 0);
  CHECK("cdef"_sv.compare(z) > 0);
}

CAF_TEST(copy) {
  char buf[10];
  auto str = "hello"_sv;
  auto n = str.copy(buf, str.size());
  CHECK_EQ(n, 5u);
  buf[n] = '\0';
  CHECK_EQ(str, string_view(buf, n));
  CHECK(strcmp("hello", buf) == 0);
  n = str.copy(buf, 10, 3);
  buf[n] = '\0';
  CHECK_EQ(string_view(buf, n), "lo");
  CHECK(strcmp("lo", buf) == 0);
}

CAF_TEST(find) {
  // Check whether string_view behaves exactly like std::string.
  auto x = "abcdef"_sv;
  std::string y = "abcdef";
  CHECK_EQ(x.find('a'), y.find('a'));
  CHECK_EQ(x.find('b'), y.find('b'));
  CHECK_EQ(x.find('g'), y.find('g'));
  CHECK_EQ(x.find('a', 1), y.find('a', 1));
  CHECK_EQ(x.find("a"), y.find("a"));
  CHECK_EQ(x.find("bc"), y.find("bc"));
  CHECK_EQ(x.find("ce"), y.find("ce"));
  CHECK_EQ(x.find("bc", 1), y.find("bc", 1));
  CHECK_EQ(x.find("bc", 1, 0), y.find("bc", 1, 0));
  CHECK_EQ(x.find("bc", 0, 1), y.find("bc", 0, 1));
  CHECK_EQ(x.find("bc", 2, 2), y.find("bc", 2, 2));
}

CAF_TEST(rfind) {
  // Check whether string_view behaves exactly like std::string.
  auto x = "abccba"_sv;
  std::string y = "abccba";
  CHECK_EQ(x.rfind('a'), y.rfind('a'));
  CHECK_EQ(x.rfind('b'), y.rfind('b'));
  CHECK_EQ(x.rfind('g'), y.rfind('g'));
  CHECK_EQ(x.rfind('a', 1), y.rfind('a', 1));
  CHECK_EQ(x.rfind("a"), y.rfind("a"));
  CHECK_EQ(x.rfind("bc"), y.rfind("bc"));
  CHECK_EQ(x.rfind("ce"), y.rfind("ce"));
  CHECK_EQ(x.rfind("bc", 1), y.rfind("bc", 1));
  CHECK_EQ(x.rfind("bc", 1, 0), y.rfind("bc", 1, 0));
  CHECK_EQ(x.rfind("bc", 0, 1), y.rfind("bc", 0, 1));
  CHECK_EQ(x.rfind("bc", 2, 2), y.rfind("bc", 2, 2));
}

CAF_TEST(find_first_of) {
  // Check whether string_view behaves exactly like std::string.
  auto x = "abcdef"_sv;
  std::string y = "abcdef";
  CHECK_EQ(x.find_first_of('a'), y.find_first_of('a'));
  CHECK_EQ(x.find_first_of('b'), y.find_first_of('b'));
  CHECK_EQ(x.find_first_of('g'), y.find_first_of('g'));
  CHECK_EQ(x.find_first_of('a', 1), y.find_first_of('a', 1));
  CHECK_EQ(x.find_first_of("a"), y.find_first_of("a"));
  CHECK_EQ(x.find_first_of("bc"), y.find_first_of("bc"));
  CHECK_EQ(x.find_first_of("ce"), y.find_first_of("ce"));
  CHECK_EQ(x.find_first_of("bc", 1), y.find_first_of("bc", 1));
  CHECK_EQ(x.find_first_of("bc", 1, 0), y.find_first_of("bc", 1, 0));
  CHECK_EQ(x.find_first_of("bc", 0, 1), y.find_first_of("bc", 0, 1));
  CHECK_EQ(x.find_first_of("bc", 2, 2), y.find_first_of("bc", 2, 2));
}

CAF_TEST(find_last_of) {
  // Check whether string_view behaves exactly like std::string.
  auto x = "abcdef"_sv;
  std::string y = "abcdef";
  CHECK_EQ(x.find_last_of('a'), y.find_last_of('a'));
  CHECK_EQ(x.find_last_of('b'), y.find_last_of('b'));
  CHECK_EQ(x.find_last_of('g'), y.find_last_of('g'));
  CHECK_EQ(x.find_last_of('a', 1), y.find_last_of('a', 1));
  CHECK_EQ(x.find_last_of("a"), y.find_last_of("a"));
  CHECK_EQ(x.find_last_of("bc"), y.find_last_of("bc"));
  CHECK_EQ(x.find_last_of("ce"), y.find_last_of("ce"));
  CHECK_EQ(x.find_last_of("bc", 1), y.find_last_of("bc", 1));
  CHECK_EQ(x.find_last_of("bc", 1, 0), y.find_last_of("bc", 1, 0));
  CHECK_EQ(x.find_last_of("bc", 0, 1), y.find_last_of("bc", 0, 1));
  CHECK_EQ(x.find_last_of("bc", 2, 2), y.find_last_of("bc", 2, 2));
}

CAF_TEST(find_first_not_of) {
  // Check whether string_view behaves exactly like std::string.
  auto x = "abcdef"_sv;
  std::string y = "abcdef";
  CHECK_EQ(x.find_first_not_of('a'), y.find_first_not_of('a'));
  CHECK_EQ(x.find_first_not_of('b'), y.find_first_not_of('b'));
  CHECK_EQ(x.find_first_not_of('g'), y.find_first_not_of('g'));
  CHECK_EQ(x.find_first_not_of('a', 1), y.find_first_not_of('a', 1));
  CHECK_EQ(x.find_first_not_of("a"), y.find_first_not_of("a"));
  CHECK_EQ(x.find_first_not_of("bc"), y.find_first_not_of("bc"));
  CHECK_EQ(x.find_first_not_of("ce"), y.find_first_not_of("ce"));
  CHECK_EQ(x.find_first_not_of("bc", 1), y.find_first_not_of("bc", 1));
  CHECK_EQ(x.find_first_not_of("bc", 1, 0), y.find_first_not_of("bc", 1, 0));
  CHECK_EQ(x.find_first_not_of("bc", 0, 1), y.find_first_not_of("bc", 0, 1));
  CHECK_EQ(x.find_first_not_of("bc", 2, 2), y.find_first_not_of("bc", 2, 2));
}

CAF_TEST(find_last_not_of) {
  // Check whether string_view behaves exactly like std::string.
  auto x = "abcdef"_sv;
  std::string y = "abcdef";
  CHECK_EQ(x.find_last_not_of('a'), y.find_last_not_of('a'));
  CHECK_EQ(x.find_last_not_of('b'), y.find_last_not_of('b'));
  CHECK_EQ(x.find_last_not_of('g'), y.find_last_not_of('g'));
  CHECK_EQ(x.find_last_not_of('a', 1), y.find_last_not_of('a', 1));
  CHECK_EQ(x.find_last_not_of("a"), y.find_last_not_of("a"));
  CHECK_EQ(x.find_last_not_of("bc"), y.find_last_not_of("bc"));
  CHECK_EQ(x.find_last_not_of("ce"), y.find_last_not_of("ce"));
  CHECK_EQ(x.find_last_not_of("bc", 1), y.find_last_not_of("bc", 1));
  CHECK_EQ(x.find_last_not_of("bc", 1, 0), y.find_last_not_of("bc", 1, 0));
  CHECK_EQ(x.find_last_not_of("bc", 0, 1), y.find_last_not_of("bc", 0, 1));
  CHECK_EQ(x.find_last_not_of("bc", 2, 2), y.find_last_not_of("bc", 2, 2));
}
