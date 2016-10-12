/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#include <utility>

#include "caf/config.hpp"

#define CAF_SUITE io_uri
#include "caf/test/unit_test.hpp"

#include "caf/all.hpp"
#include "caf/io/all.hpp"
#include "caf/io/uri.hpp"

using std::cout;
using std::cerr;
using std::endl;

using namespace caf;
using namespace caf::io;

namespace {

using str_bounds = std::pair<std::string::iterator, std::string::iterator>;

// building blocks
#define MY_SCHEME     "my_scheme"
#define MY_HOST       "my_host"
#define MY_V4_HOST    "1.2.3.4"
#define MY_V6_HOST    "2001:db8::ff00:42:8329"
#define MY_PORT       "8080"
#define MY_PATH       "my_path"
#define MY_QUERY      "my_query"
#define MY_FRAGMENT   "my_fragment"
#define MY_USER_INFO  "my_user:my_passwd"

// valid URIs
constexpr char uri_00[] = MY_SCHEME ":";
constexpr char uri_01[] = MY_SCHEME ":" MY_PATH;
constexpr char uri_02[] = MY_SCHEME ":/" MY_PATH;
constexpr char uri_03[] = MY_SCHEME "://" MY_HOST;
constexpr char uri_04[] = MY_SCHEME "://" MY_HOST ":" MY_PORT;
constexpr char uri_05[] = MY_SCHEME "://" MY_HOST ":" MY_PORT "/" MY_PATH;
constexpr char uri_06[] = MY_SCHEME "://" MY_HOST ":" MY_PORT "/" MY_PATH "?"
                          MY_QUERY;
constexpr char uri_07[] = MY_SCHEME "://" MY_HOST ":" MY_PORT "/" MY_PATH "?"
                          MY_QUERY "#" MY_FRAGMENT;
constexpr char uri_08[] = MY_SCHEME "://" MY_HOST ":" MY_PORT "?" MY_QUERY;
constexpr char uri_09[] = MY_SCHEME "://" MY_HOST ":" MY_PORT "#" MY_FRAGMENT;
constexpr char uri_10[] = MY_SCHEME "://:" MY_PORT;
constexpr char uri_11[] = MY_SCHEME "://" MY_USER_INFO "@" MY_HOST;
  
class config : public actor_system_config {
public:
  config() {
    load<io::middleman>();
    actor_system_config::parse(test::engine::argc(),
                               test::engine::argv());
  }
};

bool empty(const str_bounds& bounds) {
  return bounds.first >= bounds.second;
}
  
std::string string_of(const str_bounds& bounds) {
  return std::string(bounds.first, bounds.second);
}
  
} // namespace <anonymous>

CAF_TEST(valid_uris) {
  uri u;
  CAF_CHECK(u.empty());
  CAF_CHECK_EQUAL("", u.str());
  CAF_CHECK(empty(u.scheme()));
  CAF_CHECK(empty(u.host()));
  CAF_CHECK(empty(u.port()));
  CAF_CHECK(empty(u.path()));
  CAF_CHECK(empty(u.query()));
  CAF_CHECK(empty(u.fragment()));
  auto u_00 = uri::make(uri_00).value();
  CAF_CHECK_EQUAL(uri_00, u_00.str());
  CAF_CHECK(!empty(u_00.scheme()));
  CAF_CHECK(empty(u_00.host()));
  CAF_CHECK(empty(u_00.port()));
  CAF_CHECK(empty(u_00.path()));
  CAF_CHECK(empty(u_00.query()));
  CAF_CHECK(empty(u_00.fragment()));
  CAF_CHECK(empty(u_00.user_information()));
  CAF_CHECK_EQUAL(string_of(u_00.scheme()), MY_SCHEME);
  auto u_01 = uri::make(uri_01).value();
  CAF_CHECK_EQUAL(uri_01, u_01.str());
  CAF_CHECK(!empty(u_01.scheme()));
  CAF_CHECK(empty(u_01.host()));
  CAF_CHECK(empty(u_01.port()));
  CAF_CHECK(!empty(u_01.path()));
  CAF_CHECK(empty(u_01.query()));
  CAF_CHECK(empty(u_01.fragment()));
  CAF_CHECK(empty(u_01.user_information()));
  CAF_CHECK_EQUAL(string_of(u_01.scheme()), MY_SCHEME);
  CAF_CHECK_EQUAL(string_of(u_01.path()), MY_PATH);
  auto u_02 = uri::make(uri_02).value();
  CAF_CHECK_EQUAL(uri_02, u_02.str());
  CAF_CHECK(!empty(u_02.scheme()));
  CAF_CHECK(empty(u_02.host()));
  CAF_CHECK(empty(u_02.port()));
  CAF_CHECK(!empty(u_02.path()));
  CAF_CHECK(empty(u_02.query()));
  CAF_CHECK(empty(u_02.fragment()));
  CAF_CHECK(empty(u_02.user_information()));
  CAF_CHECK_EQUAL(string_of(u_02.scheme()), MY_SCHEME);
  CAF_CHECK_EQUAL(string_of(u_02.path()), MY_PATH);
  auto u_03 = uri::make(uri_03).value();
  CAF_CHECK_EQUAL(uri_03, u_03.str());
  CAF_CHECK(!empty(u_03.scheme()));
  CAF_CHECK(!empty(u_03.host()));
  CAF_CHECK(empty(u_03.port()));
  CAF_CHECK(empty(u_03.path()));
  CAF_CHECK(empty(u_03.query()));
  CAF_CHECK(empty(u_03.fragment()));
  CAF_CHECK(empty(u_03.user_information()));
  CAF_CHECK_EQUAL(string_of(u_03.scheme()), MY_SCHEME);
  CAF_CHECK_EQUAL(string_of(u_03.host()), MY_HOST);
  auto u_04 = uri::make(uri_04).value();
  CAF_CHECK(!empty(u_04.scheme()));
  CAF_CHECK(!empty(u_04.host()));
  CAF_CHECK(!empty(u_04.port()));
  CAF_CHECK(empty(u_04.path()));
  CAF_CHECK(empty(u_04.query()));
  CAF_CHECK(empty(u_04.fragment()));
  CAF_CHECK(empty(u_04.user_information()));
  CAF_CHECK_EQUAL(string_of(u_04.scheme()), MY_SCHEME);
  CAF_CHECK_EQUAL(string_of(u_04.host()), MY_HOST);
  CAF_CHECK_EQUAL(string_of(u_04.port()), MY_PORT);
  auto u_05 = uri::make(uri_05).value();
  CAF_CHECK_EQUAL(uri_05, u_05.str());
  CAF_CHECK(!empty(u_05.scheme()));
  CAF_CHECK(!empty(u_05.host()));
  CAF_CHECK(!empty(u_05.port()));
  CAF_CHECK(!empty(u_05.path()));
  CAF_CHECK(empty(u_05.query()));
  CAF_CHECK(empty(u_05.fragment()));
  CAF_CHECK(empty(u_05.user_information()));
  CAF_CHECK_EQUAL(string_of(u_05.scheme()), MY_SCHEME);
  CAF_CHECK_EQUAL(string_of(u_05.host()), MY_HOST);
  CAF_CHECK_EQUAL(string_of(u_05.port()), MY_PORT);
  CAF_CHECK_EQUAL(string_of(u_05.path()), MY_PATH);
  auto u_06 = uri::make(uri_06).value();
  CAF_CHECK_EQUAL(uri_06, u_06.str());
  CAF_CHECK(!empty(u_06.scheme()));
  CAF_CHECK(!empty(u_06.host()));
  CAF_CHECK(!empty(u_06.port()));
  CAF_CHECK(!empty(u_06.path()));
  CAF_CHECK(!empty(u_06.query()));
  CAF_CHECK(empty(u_06.fragment()));
  CAF_CHECK(empty(u_06.user_information()));
  CAF_CHECK_EQUAL(string_of(u_06.scheme()), MY_SCHEME);
  CAF_CHECK_EQUAL(string_of(u_06.host()), MY_HOST);
  CAF_CHECK_EQUAL(string_of(u_06.port()), MY_PORT);
  CAF_CHECK_EQUAL(string_of(u_06.path()), MY_PATH);
  CAF_CHECK_EQUAL(string_of(u_06.query()), MY_QUERY);
  auto u_07 = uri::make(uri_07).value();
  CAF_CHECK_EQUAL(uri_07, u_07.str());
  CAF_CHECK(!empty(u_07.scheme()));
  CAF_CHECK(!empty(u_07.host()));
  CAF_CHECK(!empty(u_07.port()));
  CAF_CHECK(!empty(u_07.path()));
  CAF_CHECK(!empty(u_07.query()));
  CAF_CHECK(!empty(u_07.fragment()));
  CAF_CHECK(empty(u_07.user_information()));
  CAF_CHECK_EQUAL(string_of(u_07.scheme()), MY_SCHEME);
  CAF_CHECK_EQUAL(string_of(u_07.host()), MY_HOST);
  CAF_CHECK_EQUAL(string_of(u_07.port()), MY_PORT);
  CAF_CHECK_EQUAL(string_of(u_07.path()), MY_PATH);
  CAF_CHECK_EQUAL(string_of(u_07.query()), MY_QUERY);
  CAF_CHECK_EQUAL(string_of(u_07.fragment()), MY_FRAGMENT);
  auto u_08 = uri::make(uri_08).value();
  CAF_CHECK_EQUAL(uri_08, u_08.str());
  CAF_CHECK(!empty(u_08.scheme()));
  CAF_CHECK(!empty(u_08.host()));
  CAF_CHECK(!empty(u_08.port()));
  CAF_CHECK(empty(u_08.path()));
  CAF_CHECK(!empty(u_08.query()));
  CAF_CHECK(empty(u_08.fragment()));
  CAF_CHECK(empty(u_08.user_information()));
  CAF_CHECK_EQUAL(string_of(u_08.scheme()), MY_SCHEME);
  CAF_CHECK_EQUAL(string_of(u_08.host()), MY_HOST);
  CAF_CHECK_EQUAL(string_of(u_08.port()), MY_PORT);
  CAF_CHECK_EQUAL(string_of(u_08.query()), MY_QUERY);
  auto u_09 = uri::make(uri_09).value();
  CAF_CHECK_EQUAL(uri_09, u_09.str());
  CAF_CHECK(!empty(u_09.scheme()));
  CAF_CHECK(!empty(u_09.host()));
  CAF_CHECK(!empty(u_09.port()));
  CAF_CHECK(empty(u_09.path()));
  CAF_CHECK(empty(u_09.query()));
  CAF_CHECK(!empty(u_09.fragment()));
  CAF_CHECK(empty(u_09.user_information()));
  CAF_CHECK_EQUAL(string_of(u_09.scheme()), MY_SCHEME);
  CAF_CHECK_EQUAL(string_of(u_09.host()), MY_HOST);
  CAF_CHECK_EQUAL(string_of(u_09.port()), MY_PORT);
  CAF_CHECK_EQUAL(string_of(u_09.fragment()), MY_FRAGMENT);
  auto u_10 = uri::make(uri_10).value();
  CAF_CHECK_EQUAL(uri_10, u_10.str());
  CAF_CHECK(!empty(u_10.scheme()));
  CAF_CHECK(empty(u_10.host()));
  CAF_CHECK(!empty(u_10.port()));
  CAF_CHECK(empty(u_10.path()));
  CAF_CHECK(empty(u_10.query()));
  CAF_CHECK(empty(u_10.fragment()));
  CAF_CHECK(empty(u_10.user_information()));
  CAF_CHECK_EQUAL(string_of(u_10.scheme()), MY_SCHEME);
  CAF_CHECK_EQUAL(string_of(u_10.port()), MY_PORT);
  auto u_11 = uri::make(uri_11).value();
  CAF_CHECK_EQUAL(uri_11, u_11.str());
  CAF_CHECK(!empty(u_11.scheme()));
  CAF_CHECK(!empty(u_11.host()));
  CAF_CHECK(empty(u_11.port()));
  CAF_CHECK(empty(u_11.path()));
  CAF_CHECK(empty(u_11.query()));
  CAF_CHECK(empty(u_11.fragment()));
  CAF_CHECK(!empty(u_11.user_information()));
  CAF_CHECK_EQUAL(string_of(u_11.scheme()), MY_SCHEME);
  CAF_CHECK_EQUAL(string_of(u_11.user_information()), MY_USER_INFO);
  CAF_CHECK_EQUAL(string_of(u_11.host()), MY_HOST);
}

CAF_TEST(ipv4_vs_ipv6) {
  auto u_ipv4 = uri::make(MY_SCHEME "://" MY_V4_HOST ":" MY_PORT).value();
  CAF_CHECK(u_ipv4.host_is_ipv4addr());
  CAF_CHECK_EQUAL(string_of(u_ipv4.host()), MY_V4_HOST);
  auto u_ipv6 = uri::make(MY_SCHEME "://[" MY_V6_HOST "]:" MY_PORT).value();
  CAF_CHECK(u_ipv6.host_is_ipv6addr());
  CAF_CHECK_EQUAL(string_of(u_ipv6.host()), MY_V6_HOST);
}
