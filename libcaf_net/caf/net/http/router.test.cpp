// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/http/router.hpp"

#include "caf/test/scenario.hpp"

#include "caf/net/http/server.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/octet_stream/transport.hpp"
#include "caf/net/socket.hpp"
#include "caf/net/socket_manager.hpp"

#include <source_location>

using namespace caf;
using namespace std::literals;

namespace http = caf::net::http;

using http::make_route;
using http::responder;
using http::router;

namespace {

struct response_t {
  net::http::request_header hdr;
  byte_buffer payload;

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

  error begin_chunked_message(const net::http::request_header&) override {
    return error{};
  }

  ptrdiff_t consume_chunk(const_byte_span) override {
    return 0;
  }

  error end_chunked_message() override {
    return error{};
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

auto to_str(byte_span buffer) {
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
    auto err = net::receive_timeout(fd1, 3s);
    if (err) {
      CAF_RAISE_ERROR("receive_timeout failed");
    }
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

  template <class... Ts>
  auto make_args(Ts... xs) {
    std::vector<config_value> result;
    (result.emplace_back(xs), ...);
    return result;
  }

  net::multiplexer_ptr mpx;
  net::stream_socket fd1;
  net::stream_socket fd2;
  std::thread mpx_thread;
  std::string req;
  http::request_header hdr;
  http::router rt;
  std::vector<config_value> args;
};

} // namespace

WITH_FIXTURE(fixture) {

SCENARIO("routes must have one 'arg' entry per argument") {
  auto set_get_request
    = [this](std::string path,
             std::source_location loc = std::source_location::current()) {
        req = "GET " + path + " HTTP/1.1\r\n"
              + "Host: localhost:8090\r\n"
                "User-Agent: AwesomeLib/1.0\r\n"
                "Accept-Encoding: gzip\r\n\r\n";
        auto [status, err_msg] = hdr.parse(req);
        require_eq(status, http::status::ok, loc);
      };
  auto set_post_request
    = [this](std::string path,
             std::source_location loc = std::source_location::current()) {
        req = "POST " + path + " HTTP/1.1\r\n"
              + "Host: localhost:8090\r\n"
                "User-Agent: AwesomeLib/1.0\r\n"
                "Accept-Encoding: gzip\r\n\r\n";
        auto [status, err_msg] = hdr.parse(req);
        require_eq(status, http::status::ok, loc);
      };
  GIVEN("a make_route call that has fewer arguments than the callback") {
    WHEN("evaluating the factory call") {
      THEN("the factory produces an error") {
        auto res1 = make_route("/", [](responder&, int) {});
        check_eq(res1, sec::invalid_argument);
        auto res2 = make_route("/<arg>", [](responder&, int, int) {});
        check_eq(res2, sec::invalid_argument);
      }
    }
  }
  GIVEN("a make_route call that has more arguments than the callback") {
    WHEN("evaluating the factory call") {
      THEN("the factory produces an error") {
        auto res1 = make_route("/<arg>/<arg>", [](responder&) {});
        check_eq(res1, sec::invalid_argument);
        auto res2 = make_route("/<arg>/<arg>", [](responder&, int) {});
        check_eq(res2, sec::invalid_argument);
      }
    }
  }
  GIVEN("a make_route call with the matching number of arguments") {
    WHEN("evaluating the factory and invoking it with origin-form targets") {
      THEN("the factory produces a valid callback") {
        if (auto res = make_route("/", [](responder&) {});
            check(res.has_value())) {
          set_get_request("/");
          check((*res)->exec(hdr, {}, &rt));
          set_get_request("/foo/bar");
          check(!(*res)->exec(hdr, {}, &rt));
        }
        if (auto res = make_route("/foo/bar", http::method::get,
                                  [](responder&) {});
            check(res.has_value())) {
          set_get_request("/");
          check(!(*res)->exec(hdr, {}, &rt));
          set_get_request("/foo");
          check(!(*res)->exec(hdr, {}, &rt));
          set_get_request("/foo/bar/baz");
          check(!(*res)->exec(hdr, {}, &rt));
          set_post_request("/foo/bar");
          check(!(*res)->exec(hdr, {}, &rt));
          set_get_request("/foo/bar");
          check((*res)->exec(hdr, {}, &rt));
        }
        if (auto res = make_route(
              "/<arg>", [this](responder&, int x) { args = make_args(x); });
            check(res.has_value())) {
          set_get_request("/");
          check(!(*res)->exec(hdr, {}, &rt));
          set_get_request("/foo/bar");
          check(!(*res)->exec(hdr, {}, &rt));
          set_get_request("/42");
          if (check((*res)->exec(hdr, {}, &rt)))
            check_eq(args, make_args(42));
        }
        if (auto res
            = make_route("/foo/<arg>/bar",
                         [this](responder&, int x) { args = make_args(x); });
            check(res.has_value())) {
          set_get_request("/");
          check(!(*res)->exec(hdr, {}, &rt));
          set_get_request("/foo/bar");
          check(!(*res)->exec(hdr, {}, &rt));
          set_get_request("/foo/123/bar");
          if (check((*res)->exec(hdr, {}, &rt)))
            check_eq(args, make_args(123));
        }
        if (auto res = make_route("/foo/<arg>/bar",
                                  [this](responder&, std::string x) {
                                    args = make_args(x);
                                  });
            check(res.has_value())) {
          set_get_request("/");
          check(!(*res)->exec(hdr, {}, &rt));
          set_get_request("/foo/bar");
          check(!(*res)->exec(hdr, {}, &rt));
          set_get_request("/foo/my-arg/bar");
          if (check((*res)->exec(hdr, {}, &rt)))
            check_eq(args, make_args("my-arg"s));
        }
        if (auto res = make_route("/<arg>/<arg>/<arg>",
                                  [this](responder&, int x, bool y, int z) {
                                    args = make_args(x, y, z);
                                  });
            check(res.has_value())) {
          set_get_request("/");
          check(!(*res)->exec(hdr, {}, &rt));
          set_get_request("/foo/bar");
          check(!(*res)->exec(hdr, {}, &rt));
          set_get_request("/1/true/3?foo=bar");
          if (check((*res)->exec(hdr, {}, &rt)))
            check_eq(args, make_args(1, true, 3));
        }
      }
    }
    WHEN("evaluating the factory and invoking it with absolute-form targets") {
      THEN("the factory produces a valid callback with absolute-form") {
        if (auto res = make_route("/", [](responder&) {});
            check(res.has_value())) {
          set_get_request("http://example.com");
          check((*res)->exec(hdr, {}, &rt));
          set_get_request("http://example.com/");
          check((*res)->exec(hdr, {}, &rt));
          set_get_request("http://example.com/foo/bar");
          check(!(*res)->exec(hdr, {}, &rt));
        }
        if (auto res = make_route("/foo/bar", http::method::get,
                                  [](responder&) {});
            check(res.has_value())) {
          set_get_request("http://example.com/");
          check(!(*res)->exec(hdr, {}, &rt));
          set_get_request("http://example.com/foo");
          check(!(*res)->exec(hdr, {}, &rt));
          set_get_request("http://example.com/foo/bar/baz");
          check(!(*res)->exec(hdr, {}, &rt));
          set_post_request("http://example.com/foo/bar");
          check(!(*res)->exec(hdr, {}, &rt));
          set_get_request("http://example.com/foo/bar");
          check((*res)->exec(hdr, {}, &rt));
        }
        if (auto res = make_route(
              "/<arg>", [this](responder&, int x) { args = make_args(x); });
            check(res.has_value())) {
          set_get_request("http://example.com/");
          check(!(*res)->exec(hdr, {}, &rt));
          set_get_request("http://example.com/foo/bar");
          check(!(*res)->exec(hdr, {}, &rt));
          set_get_request("http://example.com/42");
          if (check((*res)->exec(hdr, {}, &rt)))
            check_eq(args, make_args(42));
        }
        if (auto res
            = make_route("/foo/<arg>/bar",
                         [this](responder&, int x) { args = make_args(x); });
            check(res.has_value())) {
          set_get_request("http://example.com/");
          check(!(*res)->exec(hdr, {}, &rt));
          set_get_request("http://example.com/foo/bar");
          check(!(*res)->exec(hdr, {}, &rt));
          set_get_request("http://example.com/foo/123/bar");
          if (check((*res)->exec(hdr, {}, &rt)))
            check_eq(args, make_args(123));
        }
        if (auto res = make_route("/foo/<arg>/bar",
                                  [this](responder&, std::string x) {
                                    args = make_args(x);
                                  });
            check(res.has_value())) {
          set_get_request("http://example.com/");
          check(!(*res)->exec(hdr, {}, &rt));
          set_get_request("http://example.com/foo/bar");
          check(!(*res)->exec(hdr, {}, &rt));
          set_get_request("http://example.com/foo/my-arg/bar");
          if (check((*res)->exec(hdr, {}, &rt)))
            check_eq(args, make_args("my-arg"s));
        }
        if (auto res = make_route("/<arg>/<arg>/<arg>",
                                  [this](responder&, int x, bool y, int z) {
                                    args = make_args(x, y, z);
                                  });
            check(res.has_value())) {
          set_get_request("http://example.com/");
          check(!(*res)->exec(hdr, {}, &rt));
          set_get_request("http://example.com/foo/bar");
          check(!(*res)->exec(hdr, {}, &rt));
          set_get_request("http://example.com/1/true/3?foo=bar");
          if (check((*res)->exec(hdr, {}, &rt)))
            check_eq(args, make_args(1, true, 3));
        }
      }
    }
  }
}

SCENARIO("catch-all routes match any path") {
  auto set_get_request
    = [this](std::string path,
             std::source_location loc = std::source_location::current()) {
        req = "GET " + path + " HTTP/1.1\r\n"
              + "Host: localhost:8090\r\n"
                "User-Agent: AwesomeLib/1.0\r\n"
                "Accept-Encoding: gzip\r\n\r\n";
        auto [status, err_msg] = hdr.parse(req);
        require_eq(status, http::status::ok, loc);
      };
  auto set_post_request
    = [this](std::string path,
             std::source_location loc = std::source_location::current()) {
        req = "POST " + path + " HTTP/1.1\r\n"
              + "Host: localhost:8090\r\n"
                "User-Agent: AwesomeLib/1.0\r\n"
                "Accept-Encoding: gzip\r\n\r\n";
        auto [status, err_msg] = hdr.parse(req);
        require_eq(status, http::status::ok, loc);
      };
  GIVEN("a make_route call without path") {
    WHEN("evaluating the factory call") {
      THEN("the factory produces a valid callback") {
        if (auto res = make_route([](responder&) {}); check(res.has_value())) {
          set_get_request("/foo");
          check((*res)->exec(hdr, {}, &rt));
          set_post_request("/foo/bar");
          check((*res)->exec(hdr, {}, &rt));
        }
      }
    }
  }
}

SCENARIO("router converts responders to asynchronous request objects") {
  GIVEN("an HTTP router") {
    auto http_route = make_route("/foo", http::method::get, [](responder& rp) {
      rp.respond(http::status::ok, "text/plain", "Hello, World!");
    });
    auto default_router
      = router::make(std::vector<http::route_ptr>{http_route.value()});
    WHEN("responding to a request with an HTTP 200 OK response") {
      run_server(
        [this, &default_router](net::http::lower_layer::server* down,
                                const net::http::request_header& request_hdr,
                                const_byte_span body) mutable {
          auto default_responder = responder{&request_hdr, body,
                                             default_router.get()};
          auto error = default_router->start(down);
          auto req = default_router->lift(std::move(default_responder));
          req.respond(net::http::status::ok, "text/plain", "Hello, World");
          mpx->apply_updates();
        });
      THEN("the client receives the response") {
        std::string_view request = "GET /foo HTTP/1.1\r\n"
                                   "Host: localhost:8090\r\n"
                                   "User-Agent: AwesomeLib/1.0\r\n"
                                   "Accept-Encoding: gzip\r\n\r\n";
        net::write(fd1, as_bytes(std::span{request}));
        std::string_view response = "HTTP/1.1 200 OK\r\n"
                                    "Content-Type: text/plain\r\n"
                                    "Content-Length: 12\r\n\r\n"
                                    "Hello, World";
        byte_buffer buf;
        buf.resize(response.size());
        auto n = net::read(fd1, buf);
        check_eq(n, static_cast<ptrdiff_t>(response.size()));
        check_eq(to_str(buf), response);
      }
    }
    WHEN("a server discards the response promise") {
      run_server(
        [this, &default_router](net::http::lower_layer::server* down,
                                const net::http::request_header& request_hdr,
                                const_byte_span body) mutable {
          auto default_responder = responder{&request_hdr, body,
                                             default_router.get()};
          auto error = default_router->start(down);
          {
            auto req = default_router->lift(std::move(default_responder));
          }
          mpx->apply_updates();
        });
      THEN("an HTTP 500 error response is received in background") {
        std::string_view request = "GET /foo HTTP/1.1\r\n"
                                   "Host: localhost:8090\r\n"
                                   "User-Agent: AwesomeLib/1.0\r\n"
                                   "Accept-Encoding: gzip\r\n\r\n";
        net::write(fd1, as_bytes(std::span{request}));
        std::string_view response = "HTTP/1.1 500 Internal Server Error\r\n"
                                    "Content-Type: text/plain\r\n"
                                    "Content-Length: 14\r\n\r\n"
                                    "broken_promise";
        byte_buffer buf;
        buf.resize(response.size());
        auto n = net::read(fd1, buf);
        check_eq(n, static_cast<ptrdiff_t>(response.size()));
        check_eq(to_str(buf), response);
      }
    }
    WHEN("a server shuts down before responding") {
      run_server([&default_router](net::http::lower_layer::server* down,
                                   const net::http::request_header& request_hdr,
                                   const_byte_span body) mutable {
        auto default_responder = responder{&request_hdr, body,
                                           default_router.get()};
        auto error = default_router->start(down);
        auto req = default_router->lift(std::move(default_responder));
        default_router->abort_and_shutdown(make_error(sec::broken_promise));
      });
      THEN("the client becomes disconnected") {
        std::string_view request = "GET /foo HTTP/1.1\r\n"
                                   "Host: localhost:8090\r\n"
                                   "User-Agent: AwesomeLib/1.0\r\n"
                                   "Accept-Encoding: gzip\r\n\r\n";
        net::write(fd1, as_bytes(std::span{request}));
        byte_buffer buf;
        buf.resize(10);
        auto n = net::read(fd1, buf);
        check_eq(n, 0);
      }
    }
  }
}

SCENARIO("router handles chunked HTTP requests") {
  GIVEN("an HTTP router with a POST route") {
    auto http_route
      = make_route("/upload", http::method::post, [](responder& rp) {
          auto body = rp.payload();
          auto body_str = std::string_view{
            reinterpret_cast<const char*>(body.data()), body.size()};
          rp.respond(http::status::ok, "text/plain", body_str);
        });
    auto default_router
      = router::make(std::vector<http::route_ptr>{http_route.value()});
    auto server = net::http::server::make(std::move(default_router));
    auto transport = net::octet_stream::transport::make(fd2, std::move(server));
    auto mgr = net::socket_manager::make(mpx.get(), std::move(transport));
    if (!mpx->start(mgr)) {
      CAF_RAISE_ERROR(std::logic_error, "failed to start socket manager");
    }
    fd2.id = net::invalid_socket_id;
    WHEN("receiving a chunked POST request") {
      auto req_headers = "POST /upload HTTP/1.1\r\n"
                         "Host: localhost:8090\r\n"
                         "Transfer-Encoding: chunked\r\n"
                         "\r\n"sv;
      auto req_body = "D\r\n"
                      "Hello, world!\r\n"
                      "11\r\n"
                      "Developer Network\r\n"
                      "0\r\n\r\n"sv;
      net::write(fd1, as_bytes(std::span{req_headers}));
      net::write(fd1, as_bytes(std::span{req_body}));
      THEN("the router aggregates chunks and routes the complete body") {
        // Expected response body is the concatenated chunks
        std::string_view expected_response = "HTTP/1.1 200 OK\r\n"
                                             "Content-Type: text/plain\r\n"
                                             "Content-Length: 30\r\n\r\n"
                                             "Hello, world!Developer Network";
        byte_buffer buf;
        buf.resize(expected_response.size());
        auto n = net::read(fd1, buf);
        check_eq(n, static_cast<ptrdiff_t>(expected_response.size()));
        check_eq(to_str(buf), expected_response);
      }
    }
    WHEN("receiving an empty chunked POST request") {
      auto req_headers = "POST /upload HTTP/1.1\r\n"
                         "Host: localhost:8090\r\n"
                         "Transfer-Encoding: chunked\r\n"
                         "\r\n"sv;
      auto req_body = "0\r\n\r\n"sv;
      net::write(fd1, as_bytes(std::span{req_headers}));
      net::write(fd1, as_bytes(std::span{req_body}));
      THEN("the router handles empty chunked body correctly") {
        std::string_view expected_response = "HTTP/1.1 200 OK\r\n"
                                             "Content-Type: text/plain\r\n"
                                             "Content-Length: 0\r\n\r\n";
        byte_buffer buf;
        buf.resize(expected_response.size());
        auto n = net::read(fd1, buf);
        check_eq(n, static_cast<ptrdiff_t>(expected_response.size()));
        check_eq(to_str(buf), expected_response);
      }
    }
  }
}

} // WITH_FIXTURE(fixture)
