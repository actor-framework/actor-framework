// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/http/server.hpp"

#include "caf/test/scenario.hpp"
#include "caf/test/suite.hpp"

#include "caf/net/fwd.hpp"
#include "caf/net/http/request_header.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/octet_stream/transport.hpp"
#include "caf/net/socket_id.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/net/stream_socket.hpp"

#include "caf/async/promise.hpp"
#include "caf/raise_error.hpp"

using namespace caf;
using namespace std::literals;

namespace {

struct response_t {
  net::http::request_header hdr;
  byte_buffer payload;

  std::string_view payload_as_str() const noexcept {
    return {reinterpret_cast<const char*>(payload.data()), payload.size()};
  }

  std::string_view param(std::string_view key) {
    auto& qm = hdr.query();
    if (auto i = qm.find(key); i != qm.end())
      return i->second;
    else
      return {};
  }
};

class app_t : public net::http::upper_layer::server {
public:
  // -- member variables -------------------------------------------------------

  net::http::lower_layer::server* down = nullptr;

  async::promise<response_t> response;

  std::function<void(net::http::lower_layer::server*,
                     const net::http::request_header&, const_byte_span)>
    cb;

  // -- factories --------------------------------------------------------------

  template <class Callback>
  static auto make(Callback cb, async::promise<response_t> res = {}) {
    auto ptr = std::make_unique<app_t>();
    ptr->cb = std::move(cb);
    ptr->response = std::move(res);
    return ptr;
  }

  // -- implementation of http::upper_layer ------------------------------------

  error start(net::http::lower_layer::server* down_ptr) override {
    down = down_ptr;
    down->request_messages();
    return none;
  }

  void abort(const error& what) override {
    if (response) {
      response.set_error(what);
    }
  }

  void prepare_send() override {
    // nop
  }

  bool done_sending() override {
    return true;
  }

  ptrdiff_t consume(const net::http::request_header& request_hdr,
                    const_byte_span body) override {
    cb(down, request_hdr, body);
    return static_cast<ptrdiff_t>(body.size());
  }
};

auto to_str(caf::byte_span buffer) {
  return std::string_view{reinterpret_cast<const char*>(buffer.data()),
                          buffer.size()};
}

struct fixture {
  fixture() {
    mpx = net::multiplexer::make(nullptr);
    if (auto err = mpx->init()) {
      CAF_RAISE_ERROR("mpx->init failed");
    }
    mpx_thread = mpx->launch();
    auto fd_pair = net::make_stream_socket_pair();
    if (!fd_pair) {
      CAF_RAISE_ERROR("make_stream_socket_pair failed");
    }
    std::tie(fd1, fd2) = *fd_pair;
  }

  ~fixture() {
    mpx->shutdown();
    mpx_thread.join();
    if (fd1 != net::invalid_socket)
      net::close(fd1);
    if (fd2 != net::invalid_socket)
      net::close(fd2);
  }

  template <class Callback>
  void run_server(Callback cb, async::promise<response_t> res = {}) {
    auto app = app_t::make(std::move(cb), std::move(res));
    auto server = net::http::server::make(std::move(app));
    auto transport = net::octet_stream::transport::make(fd2, std::move(server));
    auto mgr = net::socket_manager::make(mpx.get(), std::move(transport));
    if (!mpx->start(mgr)) {
      CAF_RAISE_ERROR(std::logic_error, "failed to start socket manager");
    }
    fd2.id = net::invalid_socket_id;
  }

  template <class Callback>
  void run_failing_server(Callback cb, async::promise<response_t> res = {}) {
    auto app = app_t::make(std::move(cb), std::move(res));
    app->should_fail = true;
    auto server = net::http::server::make(std::move(app));
    auto transport = net::octet_stream::transport::make(fd2, std::move(server));
    auto mgr = net::socket_manager::make(mpx.get(), std::move(transport));
    if (!mpx->start(mgr)) {
      CAF_RAISE_ERROR(std::logic_error, "failed to start socket manager");
    }
    fd2.id = net::invalid_socket_id;
  }

  net::multiplexer_ptr mpx;
  net::stream_socket fd1;
  net::stream_socket fd2;
  std::thread mpx_thread;
};

} // namespace

WITH_FIXTURE(fixture) {

SCENARIO("the server parses HTTP GET requests into header fields") {
  GIVEN("valid HTTP GET request") {
    std::string_view request = "GET /foo/bar?user=foo&pw=bar HTTP/1.1\r\n"
                               "Host: localhost:8090\r\n"
                               "User-Agent: AwesomeLib/1.0\r\n"
                               "Accept-Encoding: gzip\r\n\r\n";
    std::string_view response = "HTTP/1.1 200 OK\r\n"
                                "Content-Type: text/plain\r\n"
                                "Content-Length: 12\r\n"
                                "\r\n"
                                "Hello world!";
    WHEN("sending it to an HTTP server") {
      async::promise<response_t> res_promise;
      run_server([res_promise](auto* down,
                               const net::http::request_header& request_hdr,
                               const_byte_span body) mutable {
        response_t res;
        res.hdr = request_hdr;
        res.payload.assign(body.begin(), body.end());
        res_promise.set_value(std::move(res));
        auto hello = "Hello world!"sv;
        down->send_response(net::http::status::ok, "text/plain",
                            as_bytes(make_span(hello)));
      });
      net::write(fd1, as_bytes(make_span(request)));
      THEN("the HTTP layer parses the data and calls the application layer") {
        auto maybe_res = res_promise.get_future().get(1s);
        require(maybe_res.has_value());
        auto& res = *maybe_res;
        check_eq(res.hdr.method(), net::http::method::get);
        check_eq(res.hdr.version(), "HTTP/1.1");
        check_eq(res.hdr.path(), "/foo/bar");
        check_eq(res.hdr.field("Host"), "localhost:8090");
        check_eq(res.hdr.field("User-Agent"), "AwesomeLib/1.0");
        check_eq(res.hdr.field("Accept-Encoding"), "gzip");
      }
      AND_THEN("the server sends a response from the application layer") {
        byte_buffer buf;
        buf.resize(response.size());
        net::read(fd1, buf);
        check_eq(to_str(buf), response);
      }
    }
  }
}

SCENARIO("the client receives a chunked HTTP response") {
  GIVEN("valid HTTP GET request accepting chunked encoding") {
    std::string_view request = "GET /foo/bar?user=foo&pw=bar HTTP/1.1\r\n"
                               "Host: localhost:8090\r\n"
                               "User-Agent: AwesomeLib/1.0\r\n"
                               "Accept-Encoding: chunked\r\n\r\n";
    std::string_view response = "HTTP/1.1 200 OK\r\n"
                                "Transfer-Encoding: chunked\r\n"
                                "\r\n"
                                "C\r\n"
                                "Hello world!\r\n"
                                "11\r\n"
                                "Developer Network\r\n"
                                "0\r\n"
                                "\r\n";
    WHEN("sending it to an HTTP server") {
      run_server([](auto* down, const net::http::request_header&,
                    const_byte_span) mutable {
        auto line1 = "Hello world!"sv;
        auto line2 = "Developer Network"sv;
        down->begin_header(net::http::status::ok);
        down->add_header_field("Transfer-Encoding", "chunked");
        down->end_header();
        down->send_chunk(as_bytes(make_span(line1)));
        down->send_chunk(as_bytes(make_span(line2)));
        down->send_end_of_chunks();
      });
      net::write(fd1, as_bytes(make_span(request)));
      THEN("the HTTP layer sends a chunked response to the client") {
        byte_buffer buf;
        buf.resize(response.size());
        net::read(fd1, buf);
        check_eq(to_str(buf), response);
      }
    }
  }
}

} // WITH_FIXTURE(fixture)
