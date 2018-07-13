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

#define CAF_SUITE string_view
#include "caf/test/dsl.hpp"

#include "caf/string_view.hpp"

using namespace caf;

namespace {

struct fixture {

};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(string_view_tests, fixture)

CAF_TEST(default construction) {
  string_view x;
  string_view y;
  CAF_CHECK(x.empty());
  CAF_CHECK_EQUAL(x.size(), 0u);
  CAF_CHECK_EQUAL(x.data(), nullptr);
  CAF_CHECK_EQUAL(y, y);
}

CAF_TEST(cstring conversion) {
  string_view x = "abc";
  CAF_CHECK_EQUAL(x.size(), 3u);
  CAF_CHECK_EQUAL(x[0], 'a');
  CAF_CHECK_EQUAL(x[1], 'b');
  CAF_CHECK_EQUAL(x[2], 'c');
  CAF_CHECK_EQUAL(x, "abc");
  x = "def";
  CAF_CHECK_NOT_EQUAL(x, "abc");
  CAF_CHECK_EQUAL(x, "def");
}

CAF_TEST(string conversion) {
  std::string x = "abc";
  string_view y;
  y = x;
  CAF_CHECK_EQUAL(x, y);
  auto f = [&](string_view z) {
    CAF_CHECK_EQUAL(x, z);
  };
  f(x);
}

CAF_TEST(substrings) {
  string_view x = "abcdefghi";
  CAF_CHECK(x.remove_prefix(3), "defghi");
  CAF_CHECK(x.remove_suffix(3), "abcdef");
  CAF_CHECK(x.substr(3, 3), "def");
  CAF_CHECK(x.remove_prefix(9), "");
  CAF_CHECK(x.remove_suffix(9), "");
  CAF_CHECK(x.substr(9), "");
  CAF_CHECK(x.substr(0, 0), "");
}

CAF_TEST(compare) {
  // testees
  string_view x = "abc";
  string_view y = "bcd";
  string_view z = "cde";
  // x.compare full strings
  CAF_CHECK(x.compare("abc") == 0);
  CAF_CHECK(x.compare(y) < 0);
  CAF_CHECK(x.compare(z) < 0);
  // y.compare full strings
  CAF_CHECK(y.compare(x) > 0);
  CAF_CHECK(y.compare("bcd") == 0);
  CAF_CHECK(y.compare(z) < 0);
  // z.compare full strings
  CAF_CHECK(z.compare(x) > 0);
  CAF_CHECK(z.compare(y) > 0);
  CAF_CHECK(z.compare("cde") == 0);
  // x.compare substrings
  CAF_CHECK(x.compare(0, 3, "abc") == 0);
  CAF_CHECK(x.compare(1, 2, y, 0, 2) == 0);
  CAF_CHECK(x.compare(2, 1, z, 0, 1) == 0);
  CAF_CHECK(x.compare(2, 1, z, 0, 1) == 0);
  // make sure substrings aren't equal
  CAF_CHECK(string_view("a/") != string_view("a/b"));
}

CAF_TEST(copy) {
  char buf[10];
  string_view str = "hello";
  auto n = str.copy(buf, str.size());
  CAF_CHECK_EQUAL(n, 5u);
  buf[n] = '\0';
  CAF_CHECK_EQUAL(str, string_view(buf, n));
  CAF_CHECK(strcmp("hello", buf) == 0);
  n = str.copy(buf, 10, 3);
  buf[n] = '\0';
  CAF_CHECK_EQUAL(string_view(buf, n), "lo");
  CAF_CHECK(strcmp("lo", buf) == 0);
}

CAF_TEST(find) {
  // Check whether string_view behaves exactly like std::string.
  string_view x = "abcdef";
  std::string y = "abcdef";
  CAF_CHECK_EQUAL(x.find('a'), y.find('a'));
  CAF_CHECK_EQUAL(x.find('b'), y.find('b'));
  CAF_CHECK_EQUAL(x.find('g'), y.find('g'));
  CAF_CHECK_EQUAL(x.find('a', 1), y.find('a', 1));
  CAF_CHECK_EQUAL(x.find("a"), y.find("a"));
  CAF_CHECK_EQUAL(x.find("bc"), y.find("bc"));
  CAF_CHECK_EQUAL(x.find("ce"), y.find("ce"));
  CAF_CHECK_EQUAL(x.find("bc", 1), y.find("bc", 1));
  CAF_CHECK_EQUAL(x.find("bc", 1, 0), y.find("bc", 1, 0));
  CAF_CHECK_EQUAL(x.find("bc", 0, 1), y.find("bc", 0, 1));
  CAF_CHECK_EQUAL(x.find("bc", 2, 2), y.find("bc", 2, 2));
}

CAF_TEST(rfind) {
  // Check whether string_view behaves exactly like std::string.
  string_view x = "abccba";
  std::string y = "abccba";
  CAF_CHECK_EQUAL(x.rfind('a'), y.rfind('a'));
  CAF_CHECK_EQUAL(x.rfind('b'), y.rfind('b'));
  CAF_CHECK_EQUAL(x.rfind('g'), y.rfind('g'));
  CAF_CHECK_EQUAL(x.rfind('a', 1), y.rfind('a', 1));
  CAF_CHECK_EQUAL(x.rfind("a"), y.rfind("a"));
  CAF_CHECK_EQUAL(x.rfind("bc"), y.rfind("bc"));
  CAF_CHECK_EQUAL(x.rfind("ce"), y.rfind("ce"));
  CAF_CHECK_EQUAL(x.rfind("bc", 1), y.rfind("bc", 1));
  CAF_CHECK_EQUAL(x.rfind("bc", 1, 0), y.rfind("bc", 1, 0));
  CAF_CHECK_EQUAL(x.rfind("bc", 0, 1), y.rfind("bc", 0, 1));
  CAF_CHECK_EQUAL(x.rfind("bc", 2, 2), y.rfind("bc", 2, 2));
}

CAF_TEST(find_first_of) {
  // Check whether string_view behaves exactly like std::string.
  string_view x = "abcdef";
  std::string y = "abcdef";
  CAF_CHECK_EQUAL(x.find_first_of('a'), y.find_first_of('a'));
  CAF_CHECK_EQUAL(x.find_first_of('b'), y.find_first_of('b'));
  CAF_CHECK_EQUAL(x.find_first_of('g'), y.find_first_of('g'));
  CAF_CHECK_EQUAL(x.find_first_of('a', 1), y.find_first_of('a', 1));
  CAF_CHECK_EQUAL(x.find_first_of("a"), y.find_first_of("a"));
  CAF_CHECK_EQUAL(x.find_first_of("bc"), y.find_first_of("bc"));
  CAF_CHECK_EQUAL(x.find_first_of("ce"), y.find_first_of("ce"));
  CAF_CHECK_EQUAL(x.find_first_of("bc", 1), y.find_first_of("bc", 1));
  CAF_CHECK_EQUAL(x.find_first_of("bc", 1, 0), y.find_first_of("bc", 1, 0));
  CAF_CHECK_EQUAL(x.find_first_of("bc", 0, 1), y.find_first_of("bc", 0, 1));
  CAF_CHECK_EQUAL(x.find_first_of("bc", 2, 2), y.find_first_of("bc", 2, 2));
}

CAF_TEST(find_last_of) {
  // Check whether string_view behaves exactly like std::string.
  string_view x = "abcdef";
  std::string y = "abcdef";
  CAF_CHECK_EQUAL(x.find_last_of('a'), y.find_last_of('a'));
  CAF_CHECK_EQUAL(x.find_last_of('b'), y.find_last_of('b'));
  CAF_CHECK_EQUAL(x.find_last_of('g'), y.find_last_of('g'));
  CAF_CHECK_EQUAL(x.find_last_of('a', 1), y.find_last_of('a', 1));
  CAF_CHECK_EQUAL(x.find_last_of("a"), y.find_last_of("a"));
  CAF_CHECK_EQUAL(x.find_last_of("bc"), y.find_last_of("bc"));
  CAF_CHECK_EQUAL(x.find_last_of("ce"), y.find_last_of("ce"));
  CAF_CHECK_EQUAL(x.find_last_of("bc", 1), y.find_last_of("bc", 1));
  CAF_CHECK_EQUAL(x.find_last_of("bc", 1, 0), y.find_last_of("bc", 1, 0));
  CAF_CHECK_EQUAL(x.find_last_of("bc", 0, 1), y.find_last_of("bc", 0, 1));
  CAF_CHECK_EQUAL(x.find_last_of("bc", 2, 2), y.find_last_of("bc", 2, 2));
}

CAF_TEST(find_first_not_of) {
  // Check whether string_view behaves exactly like std::string.
  string_view x = "abcdef";
  std::string y = "abcdef";
  CAF_CHECK_EQUAL(x.find_first_not_of('a'), y.find_first_not_of('a'));
  CAF_CHECK_EQUAL(x.find_first_not_of('b'), y.find_first_not_of('b'));
  CAF_CHECK_EQUAL(x.find_first_not_of('g'), y.find_first_not_of('g'));
  CAF_CHECK_EQUAL(x.find_first_not_of('a', 1), y.find_first_not_of('a', 1));
  CAF_CHECK_EQUAL(x.find_first_not_of("a"), y.find_first_not_of("a"));
  CAF_CHECK_EQUAL(x.find_first_not_of("bc"), y.find_first_not_of("bc"));
  CAF_CHECK_EQUAL(x.find_first_not_of("ce"), y.find_first_not_of("ce"));
  CAF_CHECK_EQUAL(x.find_first_not_of("bc", 1), y.find_first_not_of("bc", 1));
  CAF_CHECK_EQUAL(x.find_first_not_of("bc", 1, 0),
                  y.find_first_not_of("bc", 1, 0));
  CAF_CHECK_EQUAL(x.find_first_not_of("bc", 0, 1),
                  y.find_first_not_of("bc", 0, 1));
  CAF_CHECK_EQUAL(x.find_first_not_of("bc", 2, 2),
                  y.find_first_not_of("bc", 2, 2));
}

CAF_TEST(find_last_not_of) {
  // Check whether string_view behaves exactly like std::string.
  string_view x = "abcdef";
  std::string y = "abcdef";
  CAF_CHECK_EQUAL(x.find_last_not_of('a'), y.find_last_not_of('a'));
  CAF_CHECK_EQUAL(x.find_last_not_of('b'), y.find_last_not_of('b'));
  CAF_CHECK_EQUAL(x.find_last_not_of('g'), y.find_last_not_of('g'));
  CAF_CHECK_EQUAL(x.find_last_not_of('a', 1), y.find_last_not_of('a', 1));
  CAF_CHECK_EQUAL(x.find_last_not_of("a"), y.find_last_not_of("a"));
  CAF_CHECK_EQUAL(x.find_last_not_of("bc"), y.find_last_not_of("bc"));
  CAF_CHECK_EQUAL(x.find_last_not_of("ce"), y.find_last_not_of("ce"));
  CAF_CHECK_EQUAL(x.find_last_not_of("bc", 1), y.find_last_not_of("bc", 1));
  CAF_CHECK_EQUAL(x.find_last_not_of("bc", 1, 0),
                  y.find_last_not_of("bc", 1, 0));
  CAF_CHECK_EQUAL(x.find_last_not_of("bc", 0, 1),
                  y.find_last_not_of("bc", 0, 1));
  CAF_CHECK_EQUAL(x.find_last_not_of("bc", 2, 2),
                  y.find_last_not_of("bc", 2, 2));
}

CAF_TEST_FIXTURE_SCOPE_END()
