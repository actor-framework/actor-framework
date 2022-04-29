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

  net::http::header hdr;

  caf::byte_buffer payload;

  net::http::lower_layer* down = nullptr;

  // -- properties -------------------------------------------------------------

  std::string_view field(std::string_view key) {
    if (auto i = hdr.fields().find(key); i != hdr.fields().end())
      return i->second;
    else
      return {};
  }

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

  error init(net::socket_manager*, net::http::lower_layer* down_ptr,
             const settings&) override {
    down = down_ptr;
    down->request_messages();
    return none;
  }

  void abort(const error& reason) override {
    CAF_FAIL("app::abort called: " << reason);
  }

  bool prepare_send() override {
    return true;
  }

  bool done_sending() override {
    return true;
  }

  ptrdiff_t consume(net::http::context ctx,
                    const net::http::header& request_hdr,
                    const_byte_span body) override {
    hdr = request_hdr;
    auto content = "Hello world!"sv;
    down->send_response(ctx, net::http::status::ok, "text/plain",
                        as_bytes(make_span(content)));
    payload.assign(body.begin(), body.end());
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
      CHECK_EQ(serv->init(settings{}), error{});
      serv->push(req);
      THEN("the HTTP layer parses the data and calls the application layer") {
        CHECK_EQ(serv->handle_input(), static_cast<ptrdiff_t>(req.size()));
        auto& hdr = app->hdr;
        CHECK_EQ(hdr.method(), net::http::method::get);
        CHECK_EQ(hdr.version(), "HTTP/1.1");
        CHECK_EQ(hdr.path(), "/foo/bar");
        CHECK_EQ(app->field("Host"), "localhost:8090");
        CHECK_EQ(app->field("User-Agent"), "AwesomeLib/1.0");
        CHECK_EQ(app->field("Accept-Encoding"), "gzip");
      }
      AND("the server properly formats a response from the application layer") {
        CHECK_EQ(serv->output_as_str(), res);
      }
    }
  }
}
