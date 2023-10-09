// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE net.http.server

#include "caf/net/http/server.hpp"

#include "net-test.hpp"

using namespace caf;
using namespace std::literals;

namespace {

class app_t : public net::http::upper_layer {
public:
  // -- member variables -------------------------------------------------------

  net::http::request_header hdr;

  caf::byte_buffer payload;

  net::http::lower_layer* down = nullptr;

  bool chunked_response = false;

  // -- properties -------------------------------------------------------------

  std::string_view param(std::string_view key) {
    auto& qm = hdr.query();
    if (auto i = qm.find(key); i != qm.end())
      return i->second;
    else
      return {};
  }

  // -- factories --------------------------------------------------------------

  static auto make() {
    return std::make_unique<app_t>();
  }

  // -- implementation of http::upper_layer ------------------------------------

  error start(net::http::lower_layer* down_ptr) override {
    down = down_ptr;
    down->request_messages();
    return none;
  }

  void abort(const error& reason) override {
    CAF_FAIL("app::abort called: " << reason);
  }

  void prepare_send() override {
    // nop
  }

  bool done_sending() override {
    return true;
  }

  ptrdiff_t consume(const net::http::request_header& request_hdr,
                    const_byte_span body) override {
    hdr = request_hdr;
    payload.assign(body.begin(), body.end());
    constexpr auto content1 = "Hello world!"sv;
    constexpr auto content2 = "Developer Network"sv;
    if (chunked_response) {
      down->begin_header(net::http::status::ok);
      down->add_header_field("Transfer-Encoding", "chunked");
      down->end_header();
      down->send_chunk(as_bytes(make_span(content1)));
      down->send_chunk(as_bytes(make_span(content2)));
      down->send_end_of_chunks();
    } else {
      down->send_response(net::http::status::ok, "text/plain",
                          as_bytes(make_span(content1)));
    }
    return static_cast<ptrdiff_t>(body.size());
  }
};

} // namespace

SCENARIO("the server parses HTTP GET requests into header fields") {
  GIVEN("valid HTTP GET request") {
    std::string_view req = "GET /foo/bar?user=foo&pw=bar HTTP/1.1\r\n"
                           "Host: localhost:8090\r\n"
                           "User-Agent: AwesomeLib/1.0\r\n"
                           "Accept-Encoding: gzip\r\n\r\n";
    std::string_view res = "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/plain\r\n"
                           "Content-Length: 12\r\n"
                           "\r\n"
                           "Hello world!";
    WHEN("sending it to an HTTP server") {
      auto app_ptr = app_t::make();
      auto app = app_ptr.get();
      auto http_ptr = net::http::server::make(std::move(app_ptr));
      auto serv = mock_stream_transport::make(std::move(http_ptr));
      CHECK_EQ(serv->start(nullptr), error{});
      serv->push(req);
      THEN("the HTTP layer parses the data and calls the application layer") {
        CHECK_EQ(serv->handle_input(), static_cast<ptrdiff_t>(req.size()));
        auto& hdr = app->hdr;
        CHECK_EQ(hdr.method(), net::http::method::get);
        CHECK_EQ(hdr.version(), "HTTP/1.1");
        CHECK_EQ(hdr.path(), "/foo/bar");
        CHECK_EQ(app->hdr.field("Host"), "localhost:8090");
        CHECK_EQ(app->hdr.field("User-Agent"), "AwesomeLib/1.0");
        CHECK_EQ(app->hdr.field("Accept-Encoding"), "gzip");
      }
      AND("the server properly formats a response from the application layer") {
        CHECK_EQ(serv->output_as_str(), res);
      }
    }
  }
}

SCENARIO("the client receives a chunked HTTP response") {
  GIVEN("valid HTTP GET request accepting chunked encoding") {
    std::string_view req = "GET /foo/bar?user=foo&pw=bar HTTP/1.1\r\n"
                           "Host: localhost:8090\r\n"
                           "User-Agent: AwesomeLib/1.0\r\n"
                           "Accept-Encoding: chunked\r\n\r\n";
    std::string_view res = "HTTP/1.1 200 OK\r\n"
                           "Transfer-Encoding: chunked\r\n"
                           "\r\n"
                           "C\r\n"
                           "Hello world!\r\n"
                           "11\r\n"
                           "Developer Network\r\n"
                           "0\r\n"
                           "\r\n";
    WHEN("sending it to an HTTP server") {
      auto app_ptr = app_t::make();
      auto app = app_ptr.get();
      auto http_ptr = net::http::server::make(std::move(app_ptr));
      auto serv = mock_stream_transport::make(std::move(http_ptr));
      CHECK_EQ(serv->start(nullptr), error{});
      app->chunked_response = true;
      serv->push(req);
      THEN("the HTTP layer sends a chunked response to the client") {
        CHECK_EQ(serv->handle_input(), static_cast<ptrdiff_t>(req.size()));
        CHECK_EQ(serv->output_as_str(), res);
      }
    }
  }
}
