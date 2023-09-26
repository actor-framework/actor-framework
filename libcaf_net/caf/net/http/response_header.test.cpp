// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/http/response_header.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/test.hpp"

using namespace caf;
using namespace std::literals;

TEST("parsing a valid http response") {
  net::http::response_header hdr;
  hdr.parse("HTTP/1.1 200 OK\r\n"
            "Date: Mon, 27 Jul 2009 12:28:53 GMT\r\n"
            "Server: Apache\r\n"
            "Last-Modified: Wed, 22 Jul 2009 19:15:56 GMT\r\n"
            "Content-Length: 88\r\n"
            "Content-Type: text/html\r\n"
            "Connection: Closed\r\n"
            "\r\n");
  require(hdr.valid());
  SECTION("check version and status") {
    check_eq(hdr.version(), "HTTP/1.1");
    check_eq(hdr.status(), 200ul);
    check_eq(hdr.status_text(), "OK");
  }
  SECTION("check fields") {
    check_eq(hdr.num_fields(), 6ul);
    check_eq(hdr.field("Server"), "Apache");
    check_eq(hdr.field("Content-Length"), "88");
    check_eq(hdr.field("Content-Type"), "text/html");
  }
}

TEST("parsing a response body") {
  net::http::response_header hdr;
  hdr.parse("HTTP/1.1 200 OK\r\n"
            "Content-Length: 88\r\n"
            "Content-Type: text/html\r\n"
            "Connection: Closed\r\n"
            "\r\n"
            "This is the body");
  check(hdr.valid());
  check_eq(hdr.body(), "This is the body"sv);
}

TEST("parsing an invalid http response") {
  net::http::response_header hdr;
  SECTION("header must have the http version") {
    hdr.parse("HTTP/Foo.Bar 200 OK\r\n\r\n");
    check(!hdr.valid());
  }
  SECTION("header must have the status code") {
    hdr.parse("HTTP/1.1 Foo.Bar OK\r\n\r\n");
    check(!hdr.valid());
  }
  SECTION("header must have the status text") {
    // TODO
    // hdr.parse("HTTP/1.1 200  \r\n\r\n");
    hdr.parse("HTTP/1.1 200\r\n\r\n");
    check(!hdr.valid());
  }
  // TODO
  SECTION("header must end with an empty line") {
    hdr.parse("HTTP/1.1 200 OK\r\n\r\n");
    // check(!hdr.valid());
  }
}

TEST("rule of five") {
  net::http::response_header source;
  source.parse("HTTP/1.1 200 OK\r\n"
               "Server: Apache\r\n"
               "Content-Length: 88\r\n"
               "Content-Type: text/html\r\n"
               "\r\n"
               "This is the body");
  auto check_equality = [this](const auto& uut) {
    check_eq(uut.version(), "HTTP/1.1");
    check_eq(uut.status(), 200ul);
    check_eq(uut.status_text(), "OK");
    check_eq(uut.num_fields(), 3ul);
    check_eq(uut.field("Server"), "Apache");
    check_eq(uut.field("Content-Length"), "88");
    check_eq(uut.field("Content-Type"), "text/html");
    check_eq(uut.body(), "This is the body"sv);
  };
  check_equality(source);
  SECTION("copy ctor") {
    auto uut = source;
    check_equality(uut);
  }
  SECTION("move ctor") {
    auto uut = std::move(source);
    check_equality(uut);
  }
  net::http::response_header uut;
  SECTION("copy operator=") {
    uut = source;
    check_equality(uut);
  }
  SECTION("move operator=") {
    uut = std::move(source);
    check_equality(uut);
  }
}

CAF_TEST_MAIN()
