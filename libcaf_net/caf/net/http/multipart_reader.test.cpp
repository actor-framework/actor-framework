// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/http/multipart_reader.hpp"

#include "caf/test/test.hpp"

#include "caf/net/http/responder.hpp"
#include "caf/net/octet_stream/upper_layer.hpp"
#include "caf/net/receive_policy.hpp"

#include "caf/span.hpp"

using namespace caf;
using namespace caf::net::http;
using namespace caf::net::octet_stream;
using namespace std::literals;

TEST("multipart reader") {
  SECTION("empty multipart") {
    request_header hdr;
    hdr.parse("POST / HTTP/1.1\r\n"
              "Content-Type: multipart/form-data; boundary=test\r\n\r\n");
    auto body = std::make_shared<std::string>("--test--\r\n");
    responder res{&hdr, as_bytes(make_span(*body)), nullptr};
    multipart_reader reader{res};
    auto parts = reader.parse();
    check(parts.has_value() && parts->empty());
  }

  SECTION("single part") {
    request_header hdr;
    hdr.parse("POST / HTTP/1.1\r\n"
              "Content-Type: multipart/form-data; boundary=test\r\n\r\n");
    auto body = std::make_shared<std::string>(
      "--test\r\n"
      "Content-Disposition: form-data; name=\"field1\"\r\n\r\n"
      "value1\r\n"
      "--test--\r\n");
    responder res{&hdr, as_bytes(make_span(*body)), nullptr};
    multipart_reader reader{res};
    auto parts = reader.parse();
    check(parts.has_value());
    check_eq(parts->size(), 1u);
    check_eq(parts->front().headers, "Content-Disposition: form-data; name=\"field1\"\r\n");
    auto actual1 = std::string(reinterpret_cast<const char*>(parts->front().content.data()),
                              parts->front().content.size());
    auto expected1 = "value1";
    check_eq(actual1, expected1);
  }

  SECTION("multiple parts") {
    request_header hdr;
    hdr.parse("POST / HTTP/1.1\r\n"
              "Content-Type: multipart/form-data; boundary=test\r\n\r\n");
    auto body = std::make_shared<std::string>(
      "--test\r\n"
      "Content-Disposition: form-data; name=\"field1\"\r\n\r\n"
      "value1\r\n"
      "--test\r\n"
      "Content-Disposition: form-data; name=\"field2\"\r\n\r\n"
      "value2\r\n"
      "--test--\r\n");
    responder res{&hdr, as_bytes(make_span(*body)), nullptr};
    multipart_reader reader{res};
    auto parts = reader.parse();
    check(parts.has_value());
    check_eq(parts->size(), 2u);
    check_eq(parts->front().headers, "Content-Disposition: form-data; name=\"field1\"\r\n");
    auto actual2a = std::string(reinterpret_cast<const char*>(parts->front().content.data()),
                               parts->front().content.size());
    auto expected2a = "value1";
    check_eq(actual2a, expected2a);
    check_eq(parts->back().headers, "Content-Disposition: form-data; name=\"field2\"\r\n");
    auto actual2b = std::string(reinterpret_cast<const char*>(parts->back().content.data()),
                               parts->back().content.size());
    auto expected2b = "value2";
    check_eq(actual2b, expected2b);
  }

  SECTION("invalid boundary") {
    request_header hdr;
    hdr.parse("POST / HTTP/1.1\r\n"
              "Content-Type: multipart/form-data; boundary=test\r\n\r\n");
    auto body = std::make_shared<std::string>(
      "--wrong-boundary\r\n"
      "Content-Disposition: form-data; name=\"field1\"\r\n\r\n"
      "value1\r\n"
      "--wrong-boundary--\r\n");
    responder res{&hdr, as_bytes(make_span(*body)), nullptr};
    multipart_reader reader{res};
    auto parts = reader.parse();
    check(parts.has_value() && parts->empty());
  }

  SECTION("missing final boundary") {
    request_header hdr;
    hdr.parse("POST / HTTP/1.1\r\n"
              "Content-Type: multipart/form-data; boundary=test\r\n\r\n");
    auto body = std::make_shared<std::string>(
      "--test\r\n"
      "Content-Disposition: form-data; name=\"field1\"\r\n\r\n"
      "value1\r\n");
    responder res{&hdr, as_bytes(make_span(*body)), nullptr};
    multipart_reader reader{res};
    auto parts = reader.parse();
    check(parts.has_value() && parts->empty());
  }

  SECTION("mime type") {
    request_header hdr;
    hdr.parse("POST / HTTP/1.1\r\n"
              "Content-Type: multipart/form-data; boundary=test\r\n\r\n");
    auto body = std::make_shared<std::string>(
      "--test\r\n"
      "Content-Disposition: form-data; name=\"file\"; filename=\"test.txt\"\r\n"
      "Content-Type: text/plain\r\n\r\n"
      "Hello, World!\r\n"
      "--test--\r\n");
    responder res{&hdr, as_bytes(make_span(*body)), nullptr};
    multipart_reader reader{res};
    auto parts = reader.parse();
    check(parts.has_value());
    check_eq(parts->size(), 1u);
    check_eq(parts->front().headers,
             "Content-Disposition: form-data; name=\"file\"; filename=\"test.txt\"\r\n"
             "Content-Type: text/plain\r\n");
    auto actual3 = std::string(reinterpret_cast<const char*>(parts->front().content.data()),
                              parts->front().content.size());
    auto expected3 = "Hello, World!";
    check_eq(actual3, expected3);
  }
}
