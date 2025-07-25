// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/http/request_header.hpp"

#include "caf/test/outline.hpp"
#include "caf/test/test.hpp"

using namespace caf;
using namespace std::literals;

TEST("parsing an HTTP request") {
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

OUTLINE("parsing requests") {
  GIVEN("an HTTP request with <method> method") {
    auto method_name = block_parameters<std::string>();
    WHEN("parsing the request with origin form target") {
      auto request = detail::format("{} /foo/bar HTTP/1.1\r\n\r\n",
                                    method_name);
      net::http::request_header hdr;
      hdr.parse(request);
      THEN("the parsed <enum value> request data is valid") {
        auto expected = block_parameters<uint8_t>();
        require(hdr.valid());
        check_eq(static_cast<uint8_t>(hdr.method()), expected);
        check_eq(hdr.version(), "HTTP/1.1");
        check_eq(hdr.path(), "/foo/bar");
      }
    }
    // TODO change to WHEN block after fixing GH-1776
    AND_WHEN("parsing the request with absolute form target") {
      auto request = detail::format(
        "{} http://example.com/foo/bar HTTP/1.1\r\n\r\n", method_name);
      net::http::request_header hdr;
      hdr.parse(request);
      THEN("the parsed <enum value> request data is valid") {
        auto expected = block_parameters<uint8_t>();
        require(hdr.valid());
        check_eq(static_cast<uint8_t>(hdr.method()), expected);
        check_eq(hdr.version(), "HTTP/1.1");
        check_eq(hdr.path(), "foo/bar");
      }
    }
  }
  EXAMPLES = R"(
    |  method  | enum value |
    | GET      |     0      |
    | HEAD     |     1      |
    | POST     |     2      |
    | PUT      |     3      |
    | DELETE   |     4      |
    | OPTIONS  |     6      |
    | TRACE    |     7      |
  )";
}

TEST("parsing HTTP request with CONNECT method") {
  SECTION("request with valid authority") {
    auto request = "CONNECT node:20 HTTP/1.1\r\n\r\n"s;
    net::http::request_header hdr;
    hdr.parse(request);
    require(hdr.valid());
    check_eq(hdr.method(), net::http::method::connect);
    check_eq(hdr.version(), "HTTP/1.1");
    check_eq(hdr.path(), "");
    check_eq(hdr.authority().host, uri::host_type{"node"s});
    check_eq(hdr.authority().port, 20u);
    check(!hdr.authority().userinfo);
  }
  SECTION("request with valid authority and without port") {
    auto request = "CONNECT node HTTP/1.1\r\n\r\n"s;
    net::http::request_header hdr;
    hdr.parse(request);
    require(hdr.valid());
    check_eq(hdr.method(), net::http::method::connect);
    check_eq(hdr.version(), "HTTP/1.1");
    check_eq(hdr.path(), "");
    check_eq(hdr.authority().host, uri::host_type{"node"s});
    check_eq(hdr.authority().port, 0u);
    check(!hdr.authority().userinfo);
  }
  SECTION("request with invalid authority") {
    auto request = "CONNECT /node HTTP/1.1\r\n\r\n"s;
    net::http::request_header hdr;
    hdr.parse(request);
    require(!hdr.valid());
  }
}

TEST("parsing a server-wide HTTP OPTIONS request") {
  auto request = "OPTIONS * HTTP/1.1\r\n\r\n"s;
  net::http::request_header hdr;
  hdr.parse(request);
  require(hdr.valid());
  check_eq(hdr.method(), net::http::method::options);
  check_eq(hdr.version(), "HTTP/1.1");
  check_eq(hdr.path(), "/");
  check(!hdr.authority().userinfo);
}

TEST("parsing an invalid HTTP request") {
  net::http::request_header hdr;
  SECTION("header must have a valid HTTP method") {
    hdr.parse("EXTERMINATE /foo/bar HTTP/1.1\r\n\r\n");
    check(!hdr.valid());
  }
  SECTION("header must have the uri") {
    hdr.parse("GET \r\n\r\n");
    check(!hdr.valid());
  }
  SECTION("header must have a valid uri") {
    hdr.parse("GET foobar HTTP/1.1\r\n\r\n");
    check(!hdr.valid());
  }
  SECTION("header must end with an empty line") {
    hdr.parse("GET /foo/bar HTTP/1.1");
    check(!hdr.valid());
  }
  SECTION("empty input is invalid") {
    auto [status, text] = hdr.parse("");
    check_eq(status, net::http::status::bad_request);
    check(!hdr.valid());
  }
  SECTION("only eol is invalid") {
    hdr.parse("\r\n");
    check(!hdr.valid());
  }
  SECTION("malformed header field - missing :") {
    hdr.parse("GET /foo/bar HTTP/1.1\r\n"
              "ServerApache\r\n\r\n");
    check(!hdr.valid());
  }
  SECTION("malformed header field - empty key") {
    hdr.parse("HTTP/1.1 200 OK\r\n"
              ":Apache\r\n\r\n");
    check(!hdr.valid());
  }
}

TEST("default-constructed request headers are invalid") {
  net::http::request_header uut;
  check(!uut.valid());
  check_eq(uut.num_fields(), 0ul);
  check_eq(uut.version(), "");
  check_eq(uut.path(), "");
  check(uut.query().empty());
}

TEST("headers created by parsing empty data are invalid") {
  net::http::request_header uut;
  auto [status, _] = uut.parse("");
  check_eq(status, net::http::status::bad_request);
  check(!uut.valid());
}

TEST("request headers are copyable and movable") {
  net::http::request_header uut;
  uut.parse("GET /foo/bar?user=foo&pw=bar#baz HTTP/1.1\r\n"
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
  SECTION("copy-construction") {
    auto other{uut};
    check_equality(other);
  }
  SECTION("copy-assignment") {
    net::http::request_header other;
    other = uut;
    check_equality(other);
  }
  SECTION("move-construction") {
    auto other{std::move(uut)};
    check_equality(other);
  }
  SECTION("move-assignment") {
    net::http::request_header other;
    other = std::move(uut);
    check_equality(other);
  }
}

TEST("invalid request headers are copyable and movable") {
  auto check_invalid = [this](const auto& uut) {
    check(!uut.valid());
    check_eq(uut.num_fields(), 0ul);
    check_eq(uut.version(), "");
    check_eq(uut.path(), "");
    check(uut.query().empty());
  };
  net::http::request_header uut;
  SECTION("copy-construction") {
    auto other{uut};
    check_invalid(other);
  }
  SECTION("copy-assignment") {
    net::http::request_header other;
    other = uut;
    check_invalid(other);
  }
  SECTION("move-construction") {
    auto other{std::move(uut)};
    check_invalid(other);
  }
  SECTION("move-assignment") {
    net::http::request_header other;
    other = std::move(uut);
    check_invalid(other);
  }
}
