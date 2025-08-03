// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/http/multipart_reader.hpp"

#include "caf/test/test.hpp"

#include "caf/net/http/header.hpp"
#include "caf/net/http/request_header.hpp"
#include "caf/net/http/responder.hpp"

#include <span>

using namespace caf;
using namespace caf::net::http;
using namespace std::literals;

TEST("multipart reader") {
  request_header hdr;
  std::string body;
  std::unique_ptr<responder> res;
  auto setup = [&hdr, &body, &res](std::string_view header,
                                   std::string_view body_content) {
    hdr.parse(header);
    body = body_content;
    res = std::make_unique<responder>(&hdr, as_bytes(std::span{body}), nullptr);
  };
  SECTION("empty multipart") {
    setup("POST / HTTP/1.1\r\n"
          "Content-Type: multipart/form-data; boundary=test\r\n\r\n",
          "--test--\r\n");
    multipart_reader reader{*res};
    auto ok = false;
    auto parts = reader.parse(&ok);
    check(ok);
    check(parts.empty());
  }
  SECTION("single part") {
    setup("POST / HTTP/1.1\r\n"
          "Content-Type: multipart/form-data; boundary=test\r\n\r\n",
          "--test\r\n"
          "Content-Disposition: form-data; name=\"field1\"\r\n\r\n"
          "value1\r\n"
          "--test--\r\n");
    multipart_reader reader{*res};
    auto parts = reader.parse();
    if (check_eq(parts.size(), 1u)) {
      auto& part = parts.front();
      check_eq(part.header.field("Content-Disposition"),
               "form-data; name=\"field1\"");
      auto val = to_string_view(part.content);
      check_eq(val, "value1");
    }
  }
  SECTION("multiple parts") {
    setup("POST / HTTP/1.1\r\n"
          "Content-Type: multipart/form-data; boundary=test\r\n\r\n",
          "--test\r\n"
          "Content-Disposition: form-data; name=\"field1\"\r\n\r\n"
          "value1\r\n"
          "--test\r\n"
          "Content-Disposition: form-data; name=\"field2\"\r\n\r\n"
          "value2\r\n"
          "--test--\r\n");
    multipart_reader reader{*res};
    auto parts = reader.parse();
    check_eq(parts.size(), 2u);
    if (check_eq(parts.size(), 2u)) {
      auto& part1 = parts.front();
      check_eq(part1.header.field("Content-Disposition"),
               "form-data; name=\"field1\"");
      auto val1 = to_string_view(part1.content);
      check_eq(val1, "value1");
    }
    if (check_eq(parts.size(), 2u)) {
      auto& part2 = parts.back();
      check_eq(part2.header.field("Content-Disposition"),
               "form-data; name=\"field2\"");
      auto val2 = to_string_view(part2.content);
      check_eq(val2, "value2");
    }
  }
  SECTION("quoted boundary") {
    setup(
      "POST / HTTP/1.1\r\n"
      "Content-Type: multipart/form-data; boundary=\"test-boundary\"\r\n\r\n",
      "--test-boundary\r\n"
      "Content-Disposition: form-data; name=\"field1\"\r\n\r\n"
      "value1\r\n"
      "--test-boundary--\r\n");
    multipart_reader reader{*res};
    auto parts = reader.parse();
    if (check_eq(parts.size(), 1u)) {
      auto& part = parts.front();
      check_eq(part.header.field("Content-Disposition"),
               "form-data; name=\"field1\"");
      auto val = to_string_view(part.content);
      check_eq(val, "value1");
    }
  }
  SECTION("boundary with additional parameters") {
    setup(
      "POST / HTTP/1.1\r\n"
      "Content-Type: multipart/form-data; boundary=test; charset=utf-8\r\n\r\n",
      "--test\r\n"
      "Content-Disposition: form-data; name=\"field1\"\r\n\r\n"
      "value1\r\n"
      "--test--\r\n");
    multipart_reader reader{*res};
    auto parts = reader.parse();
    if (check_eq(parts.size(), 1u)) {
      auto& part = parts.front();
      check_eq(part.header.field("Content-Disposition"),
               "form-data; name=\"field1\"");
      auto val = to_string_view(part.content);
      check_eq(val, "value1");
    }
  }
  SECTION("invalid boundary") {
    setup("POST / HTTP/1.1\r\n"
          "Content-Type: multipart/form-data; boundary=test\r\n\r\n",
          "--wrong-boundary\r\n"
          "Content-Disposition: form-data; name=\"field1\"\r\n\r\n"
          "value1\r\n"
          "--wrong-boundary--\r\n");
    multipart_reader reader{*res};
    auto ok = false;
    auto parts = reader.parse(&ok);
    check(!ok);
    check(parts.empty());
  }
  SECTION("missing final boundary") {
    setup("POST / HTTP/1.1\r\n"
          "Content-Type: multipart/form-data; boundary=test\r\n\r\n",
          "--test\r\n"
          "Content-Disposition: form-data; name=\"field1\"\r\n\r\n"
          "value1\r\n");
    multipart_reader reader{*res};
    auto parts = reader.parse();
    check(parts.empty());
  }
  SECTION("missing boundary parameter") {
    setup("POST / HTTP/1.1\r\n"
          "Content-Type: multipart/form-data\r\n\r\n",
          "--test\r\n"
          "Content-Disposition: form-data; name=\"field1\"\r\n\r\n"
          "value1\r\n"
          "--test--\r\n");
    multipart_reader reader{*res};
    auto parts = reader.parse();
    check(parts.empty());
  }
  SECTION("empty boundary value") {
    setup("POST / HTTP/1.1\r\n"
          "Content-Type: multipart/form-data; boundary=\r\n\r\n",
          "--test\r\n"
          "Content-Disposition: form-data; name=\"field1\"\r\n\r\n"
          "value1\r\n"
          "--test--\r\n");
    multipart_reader reader{*res};
    auto parts = reader.parse();
    check(parts.empty());
  }
  SECTION("mime type") {
    setup("POST / HTTP/1.1\r\n"
          "Content-Type: multipart/form-data; boundary=test\r\n\r\n",
          "--test\r\n"
          "Content-Disposition: form-data; name=\"file\"; "
          "filename=\"test.txt\"\r\n"
          "Content-Type: text/plain\r\n\r\n"
          "Hello, World!\r\n"
          "--test--\r\n");
    multipart_reader reader{*res};
    auto parts = reader.parse();
    check_eq(parts.size(), 1u);
    if (check_eq(parts.size(), 1u)) {
      auto& part = parts.front();
      check_eq(part.header.field("Content-Disposition"),
               "form-data; name=\"file\"; filename=\"test.txt\"");
      check_eq(part.header.field("Content-Type"), "text/plain");
      auto val = to_string_view(part.content);
      check_eq(val, "Hello, World!");
    }
  }
  SECTION("non-multipart content type") {
    setup("POST / HTTP/1.1\r\n"
          "Content-Type: application/json\r\n\r\n",
          "{\"test\": \"value\"}");
    multipart_reader reader{*res};
    auto parts = reader.parse();
    check(parts.empty());
  }
  SECTION("preamble and epilogue") {
    setup("POST / HTTP/1.1\r\n"
          "Content-type: multipart/mixed; boundary=\"simple boundary\"\r\n\r\n",
          "This is the preamble.  It is to be ignored, though it\r\n"
          "is a handy place for mail composers to include an\r\n"
          "explanatory note to non-MIME compliant readers.\r\n"
          "--simple boundary\r\n"
          "\r\n"
          "This is implicitly typed plain ASCII text.\r\n"
          "It does NOT end with a linebreak.\r\n"
          "--simple boundary\r\n"
          "Content-type: text/plain; charset=us-ascii\r\n"
          "\r\n"
          "This is explicitly typed plain ASCII text.\r\n"
          "It DOES end with a linebreak.\r\n"
          "\r\n"
          "--simple boundary--\r\n"
          "This is the epilogue.  It is also to be ignored.\r\n");
    multipart_reader reader{*res};
    auto parts = reader.parse();
    check_eq(parts.size(), 2u);
    if (check_eq(parts.size(), 2u)) {
      auto& part1 = parts.front();
      auto val1 = to_string_view(part1.content);
      check_eq(val1, "This is implicitly typed plain ASCII text.\r\n"
                     "It does NOT end with a linebreak.");
      auto& part2 = parts.back();
      check_eq(part2.header.field("Content-Type"),
               "text/plain; charset=us-ascii");
      auto val2 = to_string_view(part2.content);
      check_eq(val2, "This is explicitly typed plain ASCII text.\r\n"
                     "It DOES end with a linebreak.\r\n");
    }
  }
}
