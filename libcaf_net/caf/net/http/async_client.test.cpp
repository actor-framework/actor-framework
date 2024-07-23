// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/http/async_client.hpp"

#include "caf/test/scenario.hpp"

#include "caf/net/fwd.hpp"
#include "caf/net/http/client.hpp"
#include "caf/net/http/method.hpp"
#include "caf/net/http/response.hpp"
#include "caf/net/http/status.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/octet_stream/transport.hpp"
#include "caf/net/socket_id.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/net/stream_socket.hpp"

#include "caf/raise_error.hpp"

using namespace caf;
using namespace caf::net;
using namespace std::literals;

namespace {

auto to_str(caf::const_byte_span buffer) {
  return std::string_view{reinterpret_cast<const char*>(buffer.data()),
                          buffer.size()};
}

using field_map = unordered_flat_map<std::string, std::string>;

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

  std::pair<async::future<http::response>, disposable>
  run_client(http::method method = http::method::get, std::string path = "/",
             unordered_flat_map<std::string, std::string> fields = {},
             const_byte_span payload = {}) {
    auto app = http::async_client::make(method, path, fields, payload);
    auto res = app->get_future();
    auto client = net::http::client::make(std::move(app));
    auto transport = net::octet_stream::transport::make(fd2, std::move(client));
    auto mgr = net::socket_manager::make(mpx.get(), std::move(transport));
    mpx->start(mgr);
    fd2.id = net::invalid_socket_id;
    return {res, disposable{mgr}};
  }

  net::multiplexer_ptr mpx;
  net::stream_socket fd1;
  net::stream_socket fd2;
  std::thread mpx_thread;
};

WITH_FIXTURE(fixture) {

SCENARIO("the client sends HTTP requests") {
  GIVEN("a single line HTTP request") {
    WHEN("the client sends the message") {
      auto ret = run_client(net::http::method::get, "/foo/bar/index.html"s);
      THEN("the output contains the formatted request") {
        auto want = "GET /foo/bar/index.html HTTP/1.1\r\n\r\n"sv;
        byte_buffer buf;
        buf.resize(want.size());
        auto res = net::read(fd1, buf);
        check_eq(res, static_cast<ptrdiff_t>(want.size()));
        check_eq(to_str(buf), want);
      }
    }
  }
  GIVEN("a multi-line HTTP request") {
    WHEN("the client sends the message") {
      auto ret = run_client(net::http::method::get, "/foo/bar/index.html"s,
                            field_map{{"Host", "localhost:8090"},
                                      {"User-Agent", "AwesomeLib/1.0"},
                                      {"Accept-Encoding", "chunked"}});
      THEN("the output contains the formatted request") {
        std::string_view want = "GET /foo/bar/index.html HTTP/1.1\r\n"
                                "Host: localhost:8090\r\n"
                                "User-Agent: AwesomeLib/1.0\r\n"
                                "Accept-Encoding: chunked\r\n\r\n";
        byte_buffer buf;
        buf.resize(want.size());
        auto res = net::read(fd1, buf);
        check_eq(res, static_cast<ptrdiff_t>(want.size()));
        check_eq(to_str(buf), want);
      }
    }
  }
  GIVEN("a multi-line HTTP request with a payload") {
    WHEN("the client sends the message") {
      auto body = "Hello, world!"sv;
      auto ret = run_client(net::http::method::post, "/foo/bar/index.html"s,
                            field_map{{"Content-Length", "13"},
                                      {"Content-Type", "plain/text"}},
                            as_bytes(make_span(body)));
      THEN("the output contains the formatted request") {
        std::string_view want = "POST /foo/bar/index.html HTTP/1.1\r\n"
                                "Content-Length: 13\r\n"
                                "Content-Type: plain/text\r\n"
                                "\r\n"
                                "Hello, world!";
        byte_buffer buf;
        buf.resize(want.size());
        auto res = net::read(fd1, buf);
        check_eq(res, static_cast<ptrdiff_t>(want.size()));
        check_eq(to_str(buf), want);
      }
    }
    WHEN("the client sends the message without a Content-Length") {
      auto body = "Hello, world!"sv;
      auto ret = run_client(net::http::method::post, "/foo/bar/index.html"s,
                            field_map{{"Content-Type", "plain/text"}},
                            as_bytes(make_span(body)));
      THEN("the output contains the formatted request") {
        std::string_view want = "POST /foo/bar/index.html HTTP/1.1\r\n"
                                "Content-Type: plain/text\r\n"
                                "Content-Length: 13\r\n"
                                "\r\n"
                                "Hello, world!";
        byte_buffer buf;
        buf.resize(want.size());
        auto res = net::read(fd1, buf);
        check_eq(res, static_cast<ptrdiff_t>(want.size()));
        check_eq(to_str(buf), want);
      }
    }
  }
}

SCENARIO("the client parses HTTP response into header fields") {
  GIVEN("a single line HTTP response") {
    std::string_view response = "HTTP/1.1 200 OK\r\n\r\n";
    WHEN("receiving from an HTTP server") {
      auto ret = run_client();
      net::write(fd1, as_bytes(make_span(response)));
      THEN("the HTTP layer parses the data and calls the application layer") {
        auto maybe_res = ret.first.get(1s);
        require(maybe_res.has_value());
        auto& res = *maybe_res;
        check_eq(res.code(), http::status::ok);
        check_eq(res.header_fields().size(), 0u);
        check(res.body().empty());
      }
      AND_THEN("the connection is closed") {
        check(ret.second.disposed());
      }
    }
  }
  GIVEN("a multi line HTTP response") {
    std::string_view response = "HTTP/1.1 200 OK\r\n"
                                "Content-Type: text/plain\r\n\r\n";
    WHEN("receiving from an HTTP server") {
      auto ret = run_client();
      net::write(fd1, as_bytes(make_span(response)));
      THEN("the HTTP layer parses the data and calls the application layer") {
        auto maybe_res = ret.first.get(1s);
        require(maybe_res.has_value());
        auto& res = *maybe_res;
        check_eq(res.code(), http::status::ok);
        check_eq(res.header_fields().size(), 1u);
        check_eq(res.header_fields()[0],
                 std::pair{"Content-Type"s, "text/plain"s});
        check(res.body().empty());
      }
      AND_THEN("the connection is closed") {
        check(ret.second.disposed());
      }
    }
  }
  GIVEN("a multi line HTTP response with body") {
    std::string_view response = "HTTP/1.1 200 OK\r\n"
                                "Content-Length: 13\r\n"
                                "Content-Type: text/plain\r\n\r\n"
                                "Hello, world!";
    WHEN("receiving from an HTTP server") {
      auto ret = run_client();
      net::write(fd1, as_bytes(make_span(response)));
      THEN("the HTTP layer parses the data and calls the application layer") {
        auto maybe_res = ret.first.get(1s);
        require(maybe_res.has_value());
        auto& res = *maybe_res;
        check_eq(res.code(), http::status::ok);
        auto header_fields = res.header_fields();
        check_eq(header_fields[0], std::pair{"Content-Length"s, "13"s});
        check_eq(header_fields[1], std::pair{"Content-Type"s, "text/plain"s});
        check_eq(to_str(res.body()), "Hello, world!"sv);
      }
      AND_THEN("the connection is closed") {
        check(ret.second.disposed());
      }
    }
  }
}

SCENARIO("the client receives invalid HTTP responses") {
  GIVEN("a response with an invalid header field") {
    std::string_view response = "HTTP/1.1 200 OK\r\n"
                                "FooBar\r\n"
                                "\r\n";
    WHEN("receiving from an HTTP server") {
      auto ret = run_client();
      net::write(fd1, as_bytes(make_span(response)));
      THEN("the HTTP layer parses the data and calls abort") {
        auto maybe_res = ret.first.get(1s);
        check(!maybe_res);
      }
      AND_THEN("the connection is closed") {
        check(ret.second.disposed());
      }
    }
  }
}

SCENARIO("unexpected aborts in the HTTP layer are reported as errors") {
  GIVEN("an user disposing the HTTP request") {
    WHEN("the user calls dispose") {
      auto ret = run_client();
      ret.second.dispose();
      auto maybe_res = ret.first.get(1s);
      THEN("the future is invalidated and connection closed") {
        check(ret.second.disposed());
        check(!maybe_res);
      }
    }
  }
  GIVEN("the socket connection is closed forcefully") {
    WHEN("the socket is closed") {
      auto ret = run_client();
      net::close(fd1);
      auto maybe_res = ret.first.get(1s);
      THEN("the future is invalidated and connection closed") {
        check(ret.second.disposed());
        check(!maybe_res);
      }
    }
  }
}

} // WITH_FIXTURE(fixture)

} // namespace
