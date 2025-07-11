// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/http/multipart_writer.hpp"

#include "caf/test/scenario.hpp"

#include "caf/span.hpp"

using namespace caf;
using namespace caf::net::http;
using namespace std::literals;

SCENARIO("a multipart writer accepts payloads with no headers") {
  GIVEN("a multipart_writer and a payload") {
    multipart_writer writer;
    auto payload1 = "Hello, World!"sv;
    WHEN("the payload is appended with no headers") {
      writer.append(payload1);
      auto result = to_string_view(writer.finalize());
      THEN("the result only contains the payload and the boundary") {
        std::string_view expected = "--gc0p4Jq0M2Yt08j34c0p\r\n"
                                    "\r\n"
                                    "Hello, World!\r\n"
                                    "--gc0p4Jq0M2Yt08j34c0p--\r\n";
        check_eq(result, expected);
      }
    }
  }
  GIVEN("a multipart_writer and multiple payload") {
    multipart_writer writer;
    auto payload1 = "Hello, World!"sv;
    auto payload2 = "Hello, World, again!"sv;
    auto payload3 = "Hello, World, again and again!"sv;
    WHEN("the payload is appended with no headers") {
      writer.append(payload1);
      writer.append(payload2);
      writer.append(payload3);
      auto result = to_string_view(writer.finalize());
      THEN("the result contains the payloads and the boundary") {
        std::string_view expected = "--gc0p4Jq0M2Yt08j34c0p\r\n"
                                    "\r\n"
                                    "Hello, World!\r\n"
                                    "--gc0p4Jq0M2Yt08j34c0p\r\n"
                                    "\r\n"
                                    "Hello, World, again!\r\n"
                                    "--gc0p4Jq0M2Yt08j34c0p\r\n"
                                    "\r\n"
                                    "Hello, World, again and again!\r\n"
                                    "--gc0p4Jq0M2Yt08j34c0p--\r\n";
        check_eq(result, expected);
      }
    }
  }
}

SCENARIO("a multipart writer accepts payloads with a single header field") {
  GIVEN("a multipart_writer and a payload") {
    multipart_writer writer;
    auto payload1 = "Hello, World!"sv;
    WHEN("the payload is appended with a single header field") {
      writer.append(payload1, "Content-Type", "text/plain");
      auto result = to_string_view(writer.finalize());
      THEN("the result contains the header and the payload") {
        std::string_view expected = "--gc0p4Jq0M2Yt08j34c0p\r\n"
                                    "Content-Type: text/plain\r\n"
                                    "\r\n"
                                    "Hello, World!\r\n"
                                    "--gc0p4Jq0M2Yt08j34c0p--\r\n";
        check_eq(result, expected);
      }
    }
  }
}

SCENARIO("a multipart writer accepts payloads with a header builder function") {
  GIVEN("a multipart_writer and a payload") {
    multipart_writer writer;
    auto payload1 = "Hello, World!"sv;
    WHEN("passing a function that adds no headers") {
      writer.append(payload1, [](auto&) {});
      auto result = to_string_view(writer.finalize());
      THEN("the result only contains the payload and the boundary") {
        std::string_view expected = "--gc0p4Jq0M2Yt08j34c0p\r\n"
                                    "\r\n"
                                    "Hello, World!\r\n"
                                    "--gc0p4Jq0M2Yt08j34c0p--\r\n";
        check_eq(result, expected);
      }
    }
    WHEN("passing a function that adds a single header field") {
      writer.append(payload1,
                    [](auto& w) { w.add("Content-Type", "text/plain"); });
      auto result = to_string_view(writer.finalize());
      THEN("the result contains the header and the payload") {
        std::string_view expected = "--gc0p4Jq0M2Yt08j34c0p\r\n"
                                    "Content-Type: text/plain\r\n"
                                    "\r\n"
                                    "Hello, World!\r\n"
                                    "--gc0p4Jq0M2Yt08j34c0p--\r\n";
        check_eq(result, expected);
      }
    }
    WHEN("passing a function that adds multiple header fields") {
      writer.append(payload1, [](auto& w) {
        w.add("Content-Type", "text/plain");
        w.add("Custom-Field", "FooBar");
      });
      auto result = to_string_view(writer.finalize());
      THEN("the result contains the headers and the payload") {
        std::string_view expected = "--gc0p4Jq0M2Yt08j34c0p\r\n"
                                    "Content-Type: text/plain\r\n"
                                    "Custom-Field: FooBar\r\n"
                                    "\r\n"
                                    "Hello, World!\r\n"
                                    "--gc0p4Jq0M2Yt08j34c0p--\r\n";
        check_eq(result, expected);
      }
    }
  }
}

SCENARIO("a multipart writer allows setting custom boundaries") {
  GIVEN("a multipart_writer and a payload") {
    multipart_writer writer{"custom-boundary"};
    check_eq(writer.boundary(), "custom-boundary");
    auto payload1 = "Hello, World!"sv;
    WHEN("the payload is appended with no headers") {
      writer.append(payload1);
      auto result = to_string_view(writer.finalize());
      THEN("the result only contains the payload and the boundary") {
        std::string_view expected = "--custom-boundary\r\n"
                                    "\r\n"
                                    "Hello, World!\r\n"
                                    "--custom-boundary--\r\n";
        check_eq(result, expected);
      }
    }
  }
}

SCENARIO("a multipart writer can be re-used after resetting") {
  GIVEN("a multipart_writer and two payloads") {
    multipart_writer writer;
    auto payload1 = "Hello, World!"sv;
    auto payload2 = "Hello, World, again!"sv;
    WHEN("resetting the writer after appending a payload") {
      writer.append(payload1);
      writer.reset();
      writer.append(payload2);
      auto result = to_string_view(writer.finalize());
      THEN("the result contains only what was appended after resetting") {
        std::string_view expected = "--gc0p4Jq0M2Yt08j34c0p\r\n"
                                    "\r\n"
                                    "Hello, World, again!\r\n"
                                    "--gc0p4Jq0M2Yt08j34c0p--\r\n";
        check_eq(result, expected);
      }
    }
    WHEN("resetting the writer with a new boundary") {
      writer.reset("custom-boundary");
      writer.append(payload2);
      auto result = to_string_view(writer.finalize());
      THEN("the result contains only what was appended after resetting") {
        std::string_view expected = "--custom-boundary\r\n"
                                    "\r\n"
                                    "Hello, World, again!\r\n"
                                    "--custom-boundary--\r\n";
        check_eq(result, expected);
      }
    }
  }
}
