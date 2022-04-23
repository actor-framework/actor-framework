// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE net.http.server

#include "caf/net/http/server.hpp"

#include "net-test.hpp"

using namespace caf;
using namespace caf::literals;
using namespace std::literals::string_literals;

namespace {

struct app_t {
  net::http::header hdr;

  caf::byte_buffer payload;

  template <class LowerLayerPtr>
  error init(net::socket_manager*, LowerLayerPtr, const settings&) {
    return none;
  }

  template <class LowerLayerPtr>
  bool prepare_send(LowerLayerPtr) {
    return true;
  }

  template <class LowerLayerPtr>
  bool done_sending(LowerLayerPtr) {
    return true;
  }

  template <class LowerLayerPtr>
  void abort(LowerLayerPtr, const error& reason) {
    CAF_FAIL("app::abort called: " << reason);
  }

  template <class LowerLayerPtr>
  bool consume(LowerLayerPtr down, net::http::context ctx,
               const net::http::header& request_hdr,
               caf::const_byte_span body) {
    hdr = request_hdr;
    down->send_response(ctx, net::http::status::ok, "text/plain",
                        "Hello world!");
    payload.assign(body.begin(), body.end());
    return true;
  }

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
};

using mock_server_type = mock_stream_transport<net::http::server<app_t>>;

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
      mock_server_type serv;
      CHECK_EQ(serv.init(), error{});
      serv.push(req);
      THEN("the HTTP layer parses the data and calls the application layer") {
        CHECK_EQ(serv.handle_input(), static_cast<ptrdiff_t>(req.size()));
        auto& app = serv.upper_layer.upper_layer();
        auto& hdr = app.hdr;
        CHECK_EQ(hdr.method(), net::http::method::get);
        CHECK_EQ(hdr.version(), "HTTP/1.1");
        CHECK_EQ(hdr.path(), "/foo/bar");
        CHECK_EQ(app.field("Host"), "localhost:8090");
        CHECK_EQ(app.field("User-Agent"), "AwesomeLib/1.0");
        CHECK_EQ(app.field("Accept-Encoding"), "gzip");
      }
      AND("the server properly formats a response from the application layer") {
        CHECK_EQ(serv.output_as_str(), res);
      }
    }
  }
}
