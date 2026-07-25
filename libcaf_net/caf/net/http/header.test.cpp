// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/http/header.hpp"

#include "caf/test/test.hpp"

using namespace caf;
using namespace std::literals;

TEST("parsing an HTTP request") {
  net::http::header hdr;
  hdr.parse_fields("Host: localhost:8090\r\n"
                   "User-Agent: AwesomeLib/1.0\r\n"
                   "Accept-Encoding: gzip, deflate\r\n"
                   "MyCustomField: a; b\r\n"
                   "Number: 150\r\n\r\n");
  SECTION("check value accessors") {
    check_eq(hdr.num_fields(), 5ul);
    check_eq(hdr.field("Host"), "localhost:8090");
    check_eq(hdr.field("User-Agent"), "AwesomeLib/1.0");
    check_eq(hdr.field("Accept-Encoding"), "gzip, deflate");
    check_eq(hdr.field("Number"), "150");
  }
  SECTION("fields access is case insensitive") {
    check_eq(hdr.field("HOST"), "localhost:8090");
    check_eq(hdr.field("USER-agent"), "AwesomeLib/1.0");
    check_eq(hdr.field("accept-ENCODING"), "gzip, deflate");
    check_eq(hdr.field("NUMBER"), "150");
  }
  SECTION("non-existing fields are mapped to empty strings") {
    check_eq(hdr.field("Foo"), "");
  }
  SECTION("has_token checks for a token inside a field, ignoring case") {
    check(hdr.has_token("Number", "150"));
    check(hdr.has_token("Accept-Encoding", "gzip"));
    check(hdr.has_token("Accept-Encoding", "deflate"));
    check(!hdr.has_token("Accept-Encoding", "brotli"));
    check(hdr.has_token("MyCustomField", "a", ";"));
    check(hdr.has_token("MyCustomField", "b", ";"));
    check(!hdr.has_token("MyCustomField", "c", ";"));
  }
  SECTION("field access by position") {
    using sv_pair = std::pair<std::string_view, std::string_view>;
    check_eq(hdr.field_at(0), sv_pair{"Host", "localhost:8090"});
    check_eq(hdr.field_at(1), sv_pair{"User-Agent", "AwesomeLib/1.0"});
    check_eq(hdr.field_at(2), sv_pair{"Accept-Encoding", "gzip, deflate"});
    check_eq(hdr.field_at(3), sv_pair{"MyCustomField", "a; b"});
    check_eq(hdr.field_at(4), sv_pair{"Number", "150"});
#ifdef CAF_ENABLE_EXCEPTIONS
    check_throws([&hdr]() { hdr.field_at(5); });
#endif
  }
  SECTION("has_field checks if a field exists") {
    check(hdr.has_field("HOST"));
    check(!hdr.has_field("Foo"));
  }
  SECTION("field_equals tests the content of a field") {
    check(hdr.field_equals("Host", "localhost:8090"));
    check(hdr.field_equals("HOST", "localhost:8090"));
    check(hdr.field_equals(ignore_case, "Host", "LOCALHOST:8090"));
  }
  SECTION("field_equals return false if a field doesn't exists") {
    check(!hdr.field_equals("Host", "Foo"));
    check(!hdr.field_equals("FOO", "localhost:8090"));
    check(!hdr.field_equals(ignore_case, "Host", "Foo"));
    check(!hdr.field_equals(ignore_case, "FOO", "localhost:8090"));
    check(!hdr.field_equals("Host", "LOCALHOST:8090"));
  }
  SECTION("field_as converts strings to user-defined types") {
    check_eq(hdr.field_as<int>("number"), 150);
    check_eq(hdr.field_as<float>("number"), 150.0);
    check_eq(hdr.field_as<int>("Host"), std::nullopt);
  }
  SECTION("parse_fields returns the HTTP body as remainder") {
    hdr.clear();
    auto remainder = hdr.parse_fields("Host: localhost:8090\r\n"
                                      "User-Agent: AwesomeLib/1.0\r\n"
                                      "Accept-Encoding: gzip\r\n"
                                      "Number: 150\r\n\r\n"
                                      "Remainder");
    check_eq(remainder, "Remainder");
  }
}
