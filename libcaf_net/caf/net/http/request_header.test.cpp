// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/http/request_header.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/test.hpp"

using namespace caf;
using namespace std::literals;

TEST("parsing a http request") {
  net::http::request_header hdr;
  hdr.parse("GET /foo/bar?user=foo&pw=bar#baz HTTP/1.1\r\n"
            "Host: localhost:8090\r\n"
            "User-Agent: AwesomeLib/1.0\r\n"
            "Accept-Encoding: gzip\r\n"
            "Number: 150\r\n\r\n");
  require(hdr.valid());
  SECTION("check value accessors") {
    check_eq(hdr.method(), net::http::method::get);
    check_eq(hdr.version(), "HTTP/1.1");
    check_eq(hdr.path(), "/foo/bar");
    check_eq(hdr.query().at("user"), "foo");
    check_eq(hdr.query().at("pw"), "bar");
    check_eq(hdr.fragment(), "baz");
    check_eq(hdr.num_fields(), 4ul);
    check_eq(hdr.field("Host"), "localhost:8090");
    check_eq(hdr.field("User-Agent"), "AwesomeLib/1.0");
    check_eq(hdr.field("Accept-Encoding"), "gzip");
  }
}

TEST("rule of five") {
  net::http::request_header source;
  SECTION("default constructor") {
    check(!source.valid());
    check_eq(source.num_fields(), 0ul);
    check_eq(source.version(), "");
    check_eq(source.path(), "");
    check(source.query().empty());
  }
  source.parse("GET /foo/bar?user=foo&pw=bar#baz HTTP/1.1\r\n"
               "Host: localhost:8090\r\n"
               "User-Agent: AwesomeLib/1.0\r\n"
               "Accept-Encoding: gzip\r\n"
               "Number: 150\r\n\r\n"sv);
  auto check_equality = [this](const auto& uut) {
    check_eq(uut.method(), net::http::method::get);
    check_eq(uut.version(), "HTTP/1.1");
    check_eq(uut.path(), "/foo/bar");
    check_eq(uut.query().at("user"), "foo");
    check_eq(uut.query().at("pw"), "bar");
    check_eq(uut.fragment(), "baz");
    check_eq(uut.num_fields(), 4ul);
    check_eq(uut.field("Host"), "localhost:8090");
    check_eq(uut.field("User-Agent"), "AwesomeLib/1.0");
    check_eq(uut.field("Accept-Encoding"), "gzip");
  };
  SECTION("copy constructor") {
    auto uut{source};
    check_equality(uut);
  }
  SECTION("move constructor") {
    auto uut{std::move(source)};
    check_equality(uut);
  }
  net::http::request_header uut;
  SECTION("copy assignment operator") {
    uut = source;
    check_equality(uut);
  }
  SECTION("move assignment operator") {
    uut = std::move(source);
    check_equality(uut);
  }
}

CAF_TEST_MAIN()
