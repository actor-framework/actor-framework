// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/http/response_header.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/test.hpp"

using namespace caf;
using namespace std::literals;

TEST("parsing status string") {
  auto x = "Foo.Bar"sv;
  auto cfg = config_value(x);
  net::http::status status;
  auto res = get_as<uint16_t>(cfg);
  if (auto res = get_as<uint16_t>(cfg);
      !res.has_value() || !from_integer(*res, status)) {
    check(true);
  } else {
    check(false);
  }
}

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
    check_eq(hdr.status(), caf::net::http::status::ok);
    check_eq(hdr.status_text(), "OK");
  }
  SECTION("check fields") {
    check_eq(hdr.num_fields(), 6ul);
    check_eq(hdr.field("Server"), "Apache");
    check_eq(hdr.field("Content-Length"), "88");
    check_eq(hdr.field("Content-Type"), "text/html");
  }
  SECTION("fields access is case insensitive") {
    check_eq(hdr.field("SERVER"), "Apache");
    check_eq(hdr.field("CONTENT-lENGTH"), "88");
    check_eq(hdr.field("CONTENT-TYPE"), "text/html");
  }
  SECTION("non existing field returns an empty view") {
    check_eq(hdr.field("Foo"), "");
  }
  SECTION("has_field checks if a field exists") {
    check(hdr.has_field("SERVER"));
    check(!hdr.has_field("Foo"));
  }
}

TEST("parsing a response body") {
  net::http::response_header hdr;
  auto r = hdr.parse("HTTP/1.1 200 OK\r\n"
                     "Content-Length: 88\r\n"
                     "Content-Type: text/html\r\n"
                     "Connection: Closed\r\n"
                     "\r\n"
                     "This is the body");
  print_debug(r.second);
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
    print_debug("GRESKA: {}", static_cast<int>(hdr.status()));
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

CAF_TEST_MAIN()
