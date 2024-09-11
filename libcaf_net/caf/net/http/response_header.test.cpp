// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/http/response_header.hpp"

#include "caf/test/test.hpp"

using namespace caf;
using namespace std::literals;

TEST("parsing a valid http response") {
  net::http::response_header hdr;
  SECTION("parsing a one line header") {
    hdr.parse("HTTP/1 200 OK\r\n\r\n");
    require(hdr.valid());
    check_eq(hdr.version(), "HTTP/1");
    check_eq(hdr.status(), 200ul);
    check_eq(hdr.status_text(), "OK");
    check_eq(hdr.num_fields(), 0ul);
  }
  SECTION("parsing a header with fields") {
    hdr.parse("HTTP/1.0 200 OK\r\n"
              "Date: Mon, 27 Jul 2009 12:28:53 GMT\r\n"
              "Server: Apache\r\n"
              "Last-Modified: Wed, 22 Jul 2009 19:15:56 GMT\r\n"
              "Content-Length: 88\r\n"
              "Content-Type: text/html\r\n"
              "Connection: Closed\r\n"
              "\r\n");
    require(hdr.valid());
    check_eq(hdr.version(), "HTTP/1.0");
    check_eq(hdr.status(), 200ul);
    check_eq(hdr.status_text(), "OK");
    check_eq(hdr.num_fields(), 6ul);
    check_eq(hdr.field("Server"), "Apache");
    check_eq(hdr.field("Content-Length"), "88");
    check_eq(hdr.field("Content-Type"), "text/html");
  }
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
    hdr.parse("HTTP/1.1 200  \r\n\r\n");
    check(!hdr.valid());
  }
  SECTION("header must end with an empty line") {
    hdr.parse("HTTP/1.1 200 OK\r\n");
    check(!hdr.valid());
  }
  SECTION("empty input is invalid") {
    auto [status, text] = hdr.parse("");
    check_eq(status, net::http::status::bad_request);
    check(!hdr.valid());
  }
  SECTION("only eol is an invalid") {
    hdr.parse("\r\n");
    check(!hdr.valid());
  }
  SECTION("malformed header field - missing :") {
    hdr.parse("HTTP/1.1 200 OK\r\n"
              "ServerApache\r\n\r\n");
    check(!hdr.valid());
  }
  SECTION("malformed header field - empty key") {
    hdr.parse("HTTP/1.1 200 OK\r\n"
              ":Apache\r\n\r\n");
    check(!hdr.valid());
  }
}

TEST("default-constructed response headers are invalid") {
  net::http::response_header uut;
  check(!uut.valid());
  check_eq(uut.num_fields(), 0ul);
  check_eq(uut.version(), "");
  check_eq(uut.status_text(), "");
}

TEST("response headers are copyable and movable") {
  net::http::response_header source;
  source.parse("HTTP/1.1 200 OK\r\n"
               "Server: Apache\r\n"
               "Content-Length: 88\r\n"
               "Content-Type: text/html\r\n"
               "\r\n");
  auto check_equality = [this](const auto& uut) {
    check_eq(uut.version(), "HTTP/1.1");
    check_eq(uut.status(), 200ul);
    check_eq(uut.status_text(), "OK");
    check_eq(uut.num_fields(), 3ul);
    check_eq(uut.field("Server"), "Apache");
    check_eq(uut.field("Content-Length"), "88");
    check_eq(uut.field("Content-Type"), "text/html");
  };
  check_equality(source);
  SECTION("copy-construction") {
    auto uut{source};
    check_equality(uut);
  }
  SECTION("copy-assignment") {
    net::http::response_header uut;
    uut = source;
    check_equality(uut);
  }
  SECTION("move-construction") {
    auto uut{std::move(source)};
    check_equality(uut);
  }
  SECTION("move-assignment") {
    net::http::response_header uut;
    uut = std::move(source);
    check_equality(uut);
  }
}

TEST("copying and moving invalid response header results in invalid requests") {
  auto check_invalid = [this](const auto& uut) {
    check(!uut.valid());
    check_eq(uut.num_fields(), 0ul);
    check_eq(uut.version(), "");
    check_eq(uut.status_text(), "");
  };
  net::http::response_header source;
  SECTION("copy-construction") {
    auto uut{source};
    check_invalid(uut);
  }
  SECTION("copy-assignment") {
    net::http::response_header uut;
    uut = source;
    check(!uut.valid());
  }
  SECTION("move-construction") {
    auto uut{std::move(source)};
    check(!uut.valid());
  }
  SECTION("move-assignment") {
    net::http::response_header uut;
    uut = std::move(source);
    check(!uut.valid());
  }
}
