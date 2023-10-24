// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE net.http.server

#include "caf/net/http/client.hpp"

#include "caf/test/outline.hpp"

#include "caf/net/octet_stream/lower_layer.hpp"

#include <assert.h>

using namespace caf;
using namespace std::literals;

namespace {

// Taken from legacy tests in net-test.hpp. Remove once we have a net fixture.
class mock_stream_transport : public caf::net::octet_stream::lower_layer {
public:
  // -- member types -----------------------------------------------------------

  using upper_layer_ptr = std::unique_ptr<caf::net::octet_stream::upper_layer>;

  // -- constructors, destructors, and assignment operators --------------------

  explicit mock_stream_transport(upper_layer_ptr ptr) : up(std::move(ptr)) {
    // nop
  }

  // -- factories --------------------------------------------------------------

  static std::unique_ptr<mock_stream_transport> make(upper_layer_ptr ptr) {
    return std::make_unique<mock_stream_transport>(std::move(ptr));
  }

  // -- implementation of octet_stream::lower_layer ----------------------------

  net::multiplexer& mpx() noexcept {
    return *mpx_;
  }

  bool can_send_more() const noexcept {
    return true;
  }

  bool is_reading() const noexcept {
    return max_read_size > 0;
  }

  void write_later() {
    // nop
  }

  void shutdown() {
    // nop
  }

  void switch_protocol(upper_layer_ptr new_up) {
    next.swap(new_up);
  }

  bool switching_protocol() const noexcept {
    return next != nullptr;
  }

  void configure_read(net::receive_policy policy) {
    min_read_size = policy.min_size;
    max_read_size = policy.max_size;
  }

  void begin_output() {
    // nop
  }

  byte_buffer& output_buffer() {
    return output;
  }

  std::string_view output_as_str() {
    return {reinterpret_cast<char*>(output.data()), output.size()};
  }

  bool end_output() {
    return true;
  }

  // -- initialization ---------------------------------------------------------

  caf::error start(caf::net::multiplexer* ptr) {
    mpx_ = ptr;
    return up->start(this);
  }

  // -- buffer management ------------------------------------------------------

  void push(caf::span<const std::byte> bytes) {
    input.insert(input.end(), bytes.begin(), bytes.end());
  }

  void push(std::string_view str) {
    push(caf::as_bytes(caf::make_span(str)));
  }

  size_t unconsumed() const noexcept {
    return read_buf_.size();
  }

  std::string_view output_as_str() const noexcept {
    return {reinterpret_cast<const char*>(output.data()), output.size()};
  }

  // -- event callbacks --------------------------------------------------------

  ptrdiff_t handle_input() {
    ptrdiff_t result = 0;
    auto switch_to_next_protocol = [this] {
      assert(next);
      // Switch to the new protocol and initialize it.
      configure_read(net::receive_policy::stop());
      up.reset(next.release());
      if (auto err = up->start(this)) {
        up.reset();
        return false;
      }
      return true;
    };
    // Loop until we have drained the buffer as much as we can.
    while (max_read_size > 0 && input.size() >= min_read_size) {
      auto n = std::min(input.size(), size_t{max_read_size});
      auto bytes = make_span(input.data(), n);
      auto delta = bytes.subspan(delta_offset);
      auto consumed = up->consume(bytes, delta);
      if (consumed < 0) {
        // Negative values indicate that the application encountered an
        // unrecoverable error.
        return result;
      } else if (static_cast<size_t>(consumed) > n) {
        // Must not happen. An application cannot handle more data then we pass
        // to it.
        up->abort(make_error(sec::logic_error, "consumed > buffer.size"));
        return result;
      } else if (consumed == 0) {
        if (next) {
          // When switching protocol, the new layer has never seen the data, so
          // we might just re-invoke the same data again.
          if (!switch_to_next_protocol())
            return -1;
        } else {
          // See whether the next iteration would change what we pass to the
          // application (max_read_size_ may have changed). Otherwise, we'll try
          // again later.
          delta_offset = static_cast<ptrdiff_t>(n);
          if (n == std::min(input.size(), size_t{max_read_size})) {
            return result;
          }
          // else: "Fall through".
        }
      } else {
        if (next && !switch_to_next_protocol())
          return -1;
        // Shove the unread bytes to the beginning of the buffer and continue
        // to the next loop iteration.
        result += consumed;
        delta_offset = 0;
        input.erase(input.begin(), input.begin() + consumed);
      }
    }
    return result;
  }

  // -- member variables -------------------------------------------------------

  upper_layer_ptr up;

  upper_layer_ptr next;

  caf::byte_buffer output;

  caf::byte_buffer input;

  uint32_t min_read_size = 0;

  uint32_t max_read_size = 0;

  size_t delta_offset = 0;

private:
  caf::byte_buffer read_buf_;

  caf::error abort_reason_;

  caf::net::multiplexer* mpx_;
};

class app_t : public net::http::upper_layer::client {
public:
  // -- member variables -------------------------------------------------------

  net::http::response_header hdr;

  caf::byte_buffer payload;

  net::http::lower_layer* down = nullptr;

  caf::error abort_reason;

  bool should_fail = false;

  // -- properties -------------------------------------------------------------
  std::string_view payload_as_str() const noexcept {
    return {reinterpret_cast<const char*>(payload.data()), payload.size()};
  }
  // -- factories --------------------------------------------------------------

  static auto make() {
    return std::make_unique<app_t>();
  }

  // -- implementation of http::upper_layer ------------------------------------

  error start(net::http::lower_layer::client* down_ptr) override {
    down = down_ptr;
    down->request_messages();
    return none;
  }

  void abort(const error& err) override {
    abort_reason = err;
  }

  void prepare_send() override {
    // nop
  }

  bool done_sending() override {
    return true;
  }

  ptrdiff_t consume(const net::http::response_header& response_hdr,
                    const_byte_span body) override {
    hdr = response_hdr;
    payload.assign(body.begin(), body.end());
    if (should_fail) {
      should_fail = false;
      return -1;
    }
    return static_cast<ptrdiff_t>(body.size());
  }
};

SCENARIO("the client sends HTTP requests") {
  auto app_ptr = app_t::make();
  auto client_ptr = net::http::client::make(std::move(app_ptr));
  auto client = client_ptr.get();
  auto transport = mock_stream_transport::make(std::move(client_ptr));
  require_eq(transport->start(nullptr), error{});
  GIVEN("a single line HTTP request") {
    WHEN("the client sends the message") {
      client->begin_header(net::http::method::get, "/foo/bar/index.html"sv);
      client->end_header();
      THEN("the output contains the formatted request") {
        check_eq(transport->output_as_str(),
                 "GET /foo/bar/index.html HTTP/1.1\r\n\r\n"sv);
      }
    }
  }
  GIVEN("a multi-line HTTP request") {
    auto expected = "GET /foo/bar/index.html HTTP/1.1\r\n"
                    "Host: localhost:8090\r\n"
                    "User-Agent: AwesomeLib/1.0\r\n"
                    "Accept-Encoding: chunked\r\n\r\n"sv;
    WHEN("the client sends the message") {
      client->begin_header(net::http::method::get, "/foo/bar/index.html"sv);
      client->add_header_field("Host", "localhost:8090");
      client->add_header_field("User-Agent", "AwesomeLib/1.0");
      client->add_header_field("Accept-Encoding", "chunked");
      client->end_header();
      THEN("the output contains the formatted request") {
        check_eq(transport->output_as_str(), expected);
      }
    }
  }
  GIVEN("a multi-line HTTP request with a payload") {
    auto expected = "POST /foo/bar/index.html HTTP/1.1\r\n"
                    "Content-Length: 13\r\n"
                    "Content-Type: plain/text\r\n"
                    "\r\n"
                    "Hello, world!"sv;
    WHEN("the client sends the message") {
      client->begin_header(net::http::method::post, "/foo/bar/index.html"sv);
      client->add_header_field("Content-Length", "13");
      client->add_header_field("Content-Type", "plain/text");
      client->end_header();
      client->send_payload(as_bytes(make_span("Hello, world!"sv)));
      THEN("the output contains the formatted request") {
        check_eq(transport->output_as_str(), expected);
      }
    }
  }
  GIVEN("a multi-line HTTP request with a chunked payload") {
    auto expected = "POST /foo/bar/index.html HTTP/1.1\r\n"
                    "Content-Type: plain/text\r\n"
                    "Transfer-Encoding: chunked\r\n"
                    "\r\n"
                    "D\r\n"
                    "Hello, world!\r\n"
                    "11\r\n"
                    "Developer Network\r\n"
                    "0\r\n"
                    "\r\n"sv;
    WHEN("the client sends the message") {
      client->begin_header(net::http::method::post, "/foo/bar/index.html"sv);
      client->add_header_field("Content-Type", "plain/text");
      client->add_header_field("Transfer-Encoding", "chunked");
      client->end_header();
      client->send_chunk(as_bytes(make_span("Hello, world!"sv)));
      client->send_chunk(as_bytes(make_span("Developer Network"sv)));
      client->send_end_of_chunks();
      THEN("the output contains the formatted request") {
        check_eq(transport->output_as_str(), expected);
      }
    }
  }
}

OUTLINE("Sending all available HTTP methods") {
  auto app_ptr = app_t::make();
  auto client_ptr = net::http::client::make(std::move(app_ptr));
  auto client = client_ptr.get();
  auto transport = mock_stream_transport::make(std::move(client_ptr));
  require_eq(transport->start(nullptr), error{});
  GIVEN("a http request with <method> method") {
    auto method_name = block_parameters<std::string>();
    auto expected = detail::format("{} /foo/bar HTTP/1.1\r\n\r\n", method_name);
    WHEN("the client sends the <enum value> message") {
      auto method = static_cast<net::http::method>(block_parameters<uint8_t>());
      client->begin_header(method, "/foo/bar"sv);
      client->end_header();
      THEN("the output contains the formatted request") {
        check_eq(transport->output_as_str(), expected);
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
    | CONNECT  |     5      |
    | OPTIONS  |     6      |
    | TRACE    |     7      |
  )";
}

SCENARIO("the client parses HTTP response into header fields") {
  auto app_ptr = app_t::make();
  auto app = app_ptr.get();
  auto client_ptr = net::http::client::make(std::move(app_ptr));
  auto transport = mock_stream_transport::make(std::move(client_ptr));
  require_eq(transport->start(nullptr), error{});
  GIVEN("a single line HTTP response") {
    std::string_view res = "HTTP/1.1 200 OK\r\n\r\n";
    WHEN("receiving from an HTTP server") {
      transport->push(res);
      check_eq(transport->handle_input(), static_cast<ptrdiff_t>(res.size()));
      THEN("the HTTP layer parses the data and calls the application layer") {
        check_eq(app->hdr.version(), "HTTP/1.1");
        check_eq(app->hdr.status(), 200u);
        check_eq(app->hdr.status_text(), "OK");
        check(app->payload.empty());
      }
    }
  }
  GIVEN("a multi line HTTP response") {
    std::string_view res = "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/plain\r\n\r\n";
    WHEN("receiving from an HTTP server") {
      transport->push(res);
      check_eq(transport->handle_input(), static_cast<ptrdiff_t>(res.size()));
      THEN("the HTTP layer parses the data and calls the application layer") {
        check_eq(app->hdr.version(), "HTTP/1.1");
        check_eq(app->hdr.status(), 200u);
        check_eq(app->hdr.status_text(), "OK");
        check_eq(app->hdr.field("Content-Type"), "text/plain");
        check(app->payload.empty());
      }
    }
  }
  GIVEN("a multi line HTTP response with body") {
    std::string_view res = "HTTP/1.1 200 OK\r\n"
                           "Content-Length: 13\r\n"
                           "Content-Type: text/plain\r\n\r\n"
                           "Hello, world!";
    WHEN("receiving from an HTTP server") {
      transport->push(res);
      check_eq(transport->handle_input(), static_cast<ptrdiff_t>(res.size()));
      THEN("the HTTP layer parses the data and calls the application layer") {
        check_eq(app->hdr.version(), "HTTP/1.1");
        check_eq(app->hdr.status(), 200u);
        check_eq(app->hdr.status_text(), "OK");
        check_eq(app->hdr.field("Content-Type"), "text/plain");
        check_eq(app->hdr.field("Content-Length"), "13");
        check_eq(app->payload_as_str(), "Hello, world!");
      }
    }
  }
  GIVEN("a HTTP response arriving line by line") {
    auto line1 = "HTTP/1.1 200 OK\r\n"sv;
    auto line2 = "Content-Length: 13\r\n"sv;
    auto line3 = "Content-Type: text/plain\r\n\r\n"sv;
    auto line4 = "Hello, world!"sv;
    WHEN("receiving from an HTTP server") {
      transport->push(line1);
      check_eq(transport->handle_input(), 0);
      transport->push(line2);
      check_eq(transport->handle_input(), 0);
      transport->push(line3);
      check_eq(transport->handle_input(),
               static_cast<ptrdiff_t>(line1.size() + line2.size()
                                      + line3.size()));
      transport->push(line4);
      check_eq(transport->handle_input(), static_cast<ptrdiff_t>(line4.size()));
      THEN("the HTTP layer parses the data and calls the application layer") {
        check_eq(app->hdr.version(), "HTTP/1.1");
        check_eq(app->hdr.status(), 200u);
        check_eq(app->hdr.status_text(), "OK");
        check_eq(app->hdr.field("Content-Type"), "text/plain");
        check_eq(app->hdr.field("Content-Length"), "13");
        check_eq(app->payload_as_str(), "Hello, world!");
      }
    }
  }
  GIVEN("a HTTP response containing more data than content length") {
    auto res = "HTTP/1.1 200 OK\r\n"
               "Content-Length: 13\r\n"
               "Content-Type: text/plain\r\n\r\n"
               "Hello, world!"sv;
    auto extra = "<secret>"sv;
    WHEN("receiving from an HTTP server") {
      transport->push(res);
      transport->push(extra);
      check_eq(transport->handle_input(), static_cast<ptrdiff_t>(res.size()));
      THEN("the HTTP layer parses the data and calls the application layer") {
        check_eq(app->hdr.version(), "HTTP/1.1");
        check_eq(app->hdr.status(), 200u);
        check_eq(app->hdr.status_text(), "OK");
        check_eq(app->hdr.field("Content-Type"), "text/plain");
        check_eq(app->hdr.field("Content-Length"), "13");
        check_eq(app->payload_as_str(), "Hello, world!");
      }
    }
  }
}

SCENARIO("the client receives invalid HTTP responses") {
  auto app_ptr = app_t::make();
  auto app = app_ptr.get();
  auto client_ptr = net::http::client::make(std::move(app_ptr));
  auto client = client_ptr.get();
  auto transport = mock_stream_transport::make(std::move(client_ptr));
  require_eq(transport->start(nullptr), error{});
  GIVEN("a response with an invalid header field") {
    std::string_view res = "HTTP/1.1 200 OK\r\n"
                           "FooBar\r\n"
                           "\r\n";
    WHEN("receiving from an HTTP server") {
      transport->push(res);
      check_eq(transport->handle_input(), 0);
      THEN("the HTTP layer parses the data and calls abort") {
        check(!app->abort_reason.empty());
      }
    }
  }
  GIVEN("a response that is too long") {
    std::string_view res = "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/plain\r\n\r\n";
    WHEN("receiving from an HTTP server") {
      client->max_response_size(10);
      transport->push(res);
      check_eq(transport->handle_input(), 0);
      THEN("the HTTP layer parses the data and calls abort") {
        check(!app->abort_reason.empty());
      }
    }
  }
  GIVEN("a response with a too big content length") {
    std::string_view res = "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/plain\r\n"
                           "Content-Length: 10000000\r\n\r\nHello, world";
    WHEN("receiving from an HTTP server") {
      transport->push(res);
      check_eq(transport->handle_input(), 0);
      THEN("the HTTP layer parses the data and calls abort") {
        check(!app->abort_reason.empty());
      }
    }
  }
  GIVEN("a response using chunked encoding") {
    std::string_view res = "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/plain\r\n"
                           "Transfer-Encoding: chunked\r\n"
                           "\r\n";
    WHEN("receiving from an HTTP server") {
      transport->push(res);
      check_eq(transport->handle_input(), 0);
      THEN("the HTTP layer parses the data and calls abort") {
        check(!app->abort_reason.empty());
      }
    }
  }
}

SCENARIO("apps can return errors to abort the HTTP layer") {
  auto app_ptr = app_t::make();
  auto app = app_ptr.get();
  auto client_ptr = net::http::client::make(std::move(app_ptr));
  auto transport = mock_stream_transport::make(std::move(client_ptr));
  require_eq(transport->start(nullptr), error{});
  GIVEN("an app that fails consuming the HTTP response") {
    std::string_view res = "HTTP/1.1 200 OK\r\n"
                           "FooBar\r\n"
                           "\r\n";
    app->should_fail = true;
    WHEN("the app returns -1 for the response in receives") {
      transport->push(res);
      check_eq(transport->handle_input(), 0);
      THEN("the client calls abort") {
        check(!app->abort_reason.empty());
      }
    }
  }
}

} // namespace
