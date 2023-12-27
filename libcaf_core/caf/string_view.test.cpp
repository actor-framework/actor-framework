// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/string_view.hpp"

#include "caf/test/test.hpp"

using namespace caf;
using namespace caf::literals;

namespace {

CAF_PUSH_DEPRECATED_WARNING

TEST("default construction") {
  string_view x;
  string_view y;
  check(x.empty());
  check_eq(x.size(), 0u);
  check_eq(x.data(), nullptr);
  check_eq(y, y);
}

TEST("cstring conversion") {
  auto x = "abc"_sv;
  check_eq(x.size(), 3u);
  check_eq(x[0], 'a');
  check_eq(x[1], 'b');
  check_eq(x[2], 'c');
  check_eq(x, "abc");
  x = "def";
  check_ne(x, "abc");
  check_eq(x, "def");
}

TEST("string conversion") {
  std::string x = "abc";
  string_view y;
  y = x;
  check_eq(x, y);
  auto f = [&](string_view z) { check_eq(x, z); };
  f(x);
}

TEST("substrings") {
  auto without_prefix = [](string_view str, size_t n) {
    str.remove_prefix(n);
    return str;
  };
  auto without_suffix = [](string_view str, size_t n) {
    str.remove_suffix(n);
    return str;
  };
  auto x = "abcdefghi"_sv;
  check_eq(without_prefix(x, 3), "defghi");
  check_eq(without_suffix(x, 3), "abcdef");
  check_eq(x.substr(3, 3), "def");
  check_eq(without_prefix(x, 9), "");
  check_eq(without_suffix(x, 9), "");
  check_eq(x.substr(9), "");
  check_eq(x.substr(0, 0), "");
}

TEST("compare") {
  // testees
  auto x = "abc"_sv;
  auto y = "bcd"_sv;
  auto z = "cde"_sv;
  // x.compare full strings
  check(x.compare("abc") == 0);
  check(x.compare(y) < 0);
  check(x.compare(z) < 0);
  // y.compare full strings
  check(y.compare(x) > 0);
  check(y.compare("bcd") == 0);
  check(y.compare(z) < 0);
  // z.compare full strings
  check(z.compare(x) > 0);
  check(z.compare(y) > 0);
  check(z.compare("cde") == 0);
  // x.compare substrings
  check(x.compare(0, 3, "abc") == 0);
  check(x.compare(1, 2, y, 0, 2) == 0);
  check(x.compare(2, 1, z, 0, 1) == 0);
  // make sure substrings aren't equal
  check("a/"_sv != "a/b"_sv);
  check(z.compare("cdef"_sv) < 0);
  check("cdef"_sv.compare(z) > 0);
}

TEST("copy") {
  char buf[10];
  auto str = "hello"_sv;
  auto n = str.copy(buf, str.size());
  check_eq(n, 5u);
  buf[n] = '\0';
  check_eq(str, string_view(buf, n));
  check(strcmp("hello", buf) == 0);
  n = str.copy(buf, 10, 3);
  buf[n] = '\0';
  check_eq(string_view(buf, n), "lo");
  check(strcmp("lo", buf) == 0);
}

TEST("find") {
  // Check whether string_view behaves exactly like std::string.
  auto x = "abcdef"_sv;
  std::string y = "abcdef";
  check_eq(x.find('a'), y.find('a'));
  check_eq(x.find('b'), y.find('b'));
  check_eq(x.find('g'), y.find('g'));
  check_eq(x.find('a', 1), y.find('a', 1));
  check_eq(x.find("a"), y.find("a"));
  check_eq(x.find("bc"), y.find("bc"));
  check_eq(x.find("ce"), y.find("ce"));
  check_eq(x.find("bc", 1), y.find("bc", 1));
  check_eq(x.find("bc", 1, 0), y.find("bc", 1, 0));
  check_eq(x.find("bc", 0, 1), y.find("bc", 0, 1));
  check_eq(x.find("bc", 2, 2), y.find("bc", 2, 2));
}

TEST("rfind") {
  // Check whether string_view behaves exactly like std::string.
  auto x = "abccba"_sv;
  std::string y = "abccba";
  check_eq(x.rfind('a'), y.rfind('a'));
  check_eq(x.rfind('b'), y.rfind('b'));
  check_eq(x.rfind('g'), y.rfind('g'));
  check_eq(x.rfind('a', 1), y.rfind('a', 1));
  check_eq(x.rfind("a"), y.rfind("a"));
  check_eq(x.rfind("bc"), y.rfind("bc"));
  check_eq(x.rfind("ce"), y.rfind("ce"));
  check_eq(x.rfind("bc", 1), y.rfind("bc", 1));
  check_eq(x.rfind("bc", 1, 0), y.rfind("bc", 1, 0));
  check_eq(x.rfind("bc", 0, 1), y.rfind("bc", 0, 1));
  check_eq(x.rfind("bc", 2, 2), y.rfind("bc", 2, 2));
}

TEST("find_first_of") {
  // Check whether string_view behaves exactly like std::string.
  auto x = "abcdef"_sv;
  std::string y = "abcdef";
  check_eq(x.find_first_of('a'), y.find_first_of('a'));
  check_eq(x.find_first_of('b'), y.find_first_of('b'));
  check_eq(x.find_first_of('g'), y.find_first_of('g'));
  check_eq(x.find_first_of('a', 1), y.find_first_of('a', 1));
  check_eq(x.find_first_of("a"), y.find_first_of("a"));
  check_eq(x.find_first_of("bc"), y.find_first_of("bc"));
  check_eq(x.find_first_of("ce"), y.find_first_of("ce"));
  check_eq(x.find_first_of("bc", 1), y.find_first_of("bc", 1));
  check_eq(x.find_first_of("bc", 1, 0), y.find_first_of("bc", 1, 0));
  check_eq(x.find_first_of("bc", 0, 1), y.find_first_of("bc", 0, 1));
  check_eq(x.find_first_of("bc", 2, 2), y.find_first_of("bc", 2, 2));
}

TEST("find_last_of") {
  // Check whether string_view behaves exactly like std::string.
  auto x = "abcdef"_sv;
  std::string y = "abcdef";
  check_eq(x.find_last_of('a'), y.find_last_of('a'));
  check_eq(x.find_last_of('b'), y.find_last_of('b'));
  check_eq(x.find_last_of('g'), y.find_last_of('g'));
  check_eq(x.find_last_of('a', 1), y.find_last_of('a', 1));
  check_eq(x.find_last_of("a"), y.find_last_of("a"));
  check_eq(x.find_last_of("bc"), y.find_last_of("bc"));
  check_eq(x.find_last_of("ce"), y.find_last_of("ce"));
  check_eq(x.find_last_of("bc", 1), y.find_last_of("bc", 1));
  check_eq(x.find_last_of("bc", 1, 0), y.find_last_of("bc", 1, 0));
  check_eq(x.find_last_of("bc", 0, 1), y.find_last_of("bc", 0, 1));
  check_eq(x.find_last_of("bc", 2, 2), y.find_last_of("bc", 2, 2));
}

TEST("find_first_not_of") {
  // Check whether string_view behaves exactly like std::string.
  auto x = "abcdef"_sv;
  std::string y = "abcdef";
  check_eq(x.find_first_not_of('a'), y.find_first_not_of('a'));
  check_eq(x.find_first_not_of('b'), y.find_first_not_of('b'));
  check_eq(x.find_first_not_of('g'), y.find_first_not_of('g'));
  check_eq(x.find_first_not_of('a', 1), y.find_first_not_of('a', 1));
  check_eq(x.find_first_not_of("a"), y.find_first_not_of("a"));
  check_eq(x.find_first_not_of("bc"), y.find_first_not_of("bc"));
  check_eq(x.find_first_not_of("ce"), y.find_first_not_of("ce"));
  check_eq(x.find_first_not_of("bc", 1), y.find_first_not_of("bc", 1));
  check_eq(x.find_first_not_of("bc", 1, 0), y.find_first_not_of("bc", 1, 0));
  check_eq(x.find_first_not_of("bc", 0, 1), y.find_first_not_of("bc", 0, 1));
  check_eq(x.find_first_not_of("bc", 2, 2), y.find_first_not_of("bc", 2, 2));
}

TEST("find_last_not_of") {
  // Check whether string_view behaves exactly like std::string.
  auto x = "abcdef"_sv;
  std::string y = "abcdef";
  check_eq(x.find_last_not_of('a'), y.find_last_not_of('a'));
  check_eq(x.find_last_not_of('b'), y.find_last_not_of('b'));
  check_eq(x.find_last_not_of('g'), y.find_last_not_of('g'));
  check_eq(x.find_last_not_of('a', 1), y.find_last_not_of('a', 1));
  check_eq(x.find_last_not_of("a"), y.find_last_not_of("a"));
  check_eq(x.find_last_not_of("bc"), y.find_last_not_of("bc"));
  check_eq(x.find_last_not_of("ce"), y.find_last_not_of("ce"));
  check_eq(x.find_last_not_of("bc", 1), y.find_last_not_of("bc", 1));
  check_eq(x.find_last_not_of("bc", 1, 0), y.find_last_not_of("bc", 1, 0));
  check_eq(x.find_last_not_of("bc", 0, 1), y.find_last_not_of("bc", 0, 1));
  check_eq(x.find_last_not_of("bc", 2, 2), y.find_last_not_of("bc", 2, 2));
}

CAF_POP_WARNINGS

} // namespace
