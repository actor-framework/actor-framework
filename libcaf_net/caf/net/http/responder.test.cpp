// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/http/responder.hpp"

#include "caf/test/runnable.hpp"
#include "caf/test/test.hpp"

#include "caf/net/http/response_header.hpp"
#include "caf/net/http/router.hpp"
#include "caf/net/http/server.hpp"
#include "caf/net/http/v1.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/octet_stream/transport.hpp"
#include "caf/net/socket.hpp"
#include "caf/net/socket_guard.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/net/stream_socket.hpp"

#include "caf/detail/connection_guard.hpp"

#include <atomic>
#include <cctype>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <vector>

using namespace caf;
using namespace std::literals;

namespace http = caf::net::http;

namespace {

class nil_guard : public detail::connection_guard {
public:
  bool orphaned() const noexcept override {
    return orphaned_.load(std::memory_order_acquire);
  }

  void set_orphaned() noexcept override {
    orphaned_.store(true, std::memory_order_release);
  }

private:
  std::atomic<bool> orphaned_{false};
};

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
    sguard1.reset(fd_pair->first);
    sguard2.reset(fd_pair->second);
    auto err = net::receive_timeout(sguard1.get(), 1s);
    if (err) {
      CAF_RAISE_ERROR("receive_timeout failed");
    }
  }

  ~fixture() {
    server.dispose();
    mpx->shutdown();
    mpx_thread.join();
  }

  void launch() {
    // Instantiate our protocol stack.
    auto app = net::http::router::make(routes, make_counted<nil_guard>());
    auto serv = net::http::server::make(std::move(app));
    serv->max_request_size(8192);
    auto transport = net::octet_stream::transport::make(sguard2.release(),
                                                        std::move(serv));
    transport->max_consecutive_reads(1024);
    transport->active_policy().accept();
    auto res = net::socket_manager::make(mpx.get(), std::move(transport));
    server = res->as_disposable();
    test::runnable::current().require(mpx->start(res));
  }

  template <class Fn>
  void route(std::string path, Fn fn) {
    auto maybe_route = http::make_route(std::move(path), std::move(fn));
    test::runnable::current().require(maybe_route.has_value());
    routes.emplace_back(std::move(*maybe_route));
  }

  void
  send_request(http::method method, std::string_view path,
               std::source_location loc = std::source_location::current()) {
    auto& ctx = test::runnable::current();
    byte_buffer buf;
    http::v1::begin_request_header(method, path, buf);
    ctx.require(http::v1::end_header(buf), loc);
    auto res = net::write(sguard1.get(), buf);
    ctx.require_eq(res, static_cast<ptrdiff_t>(buf.size()), loc);
  }

  std::tuple<http::status, std::map<std::string, std::string>, byte_buffer>
  receive_response(std::source_location loc = std::source_location::current()) {
    auto& ctx = test::runnable::current();
    byte_buffer buf;
    buf.resize(1024 * 1024);
    auto n = net::read(sguard1.get(), buf);
    ctx.require_gt(n, 0, loc);
    buf.resize(static_cast<size_t>(n));
    auto [hdr_str, payload] = http::v1::split_header(buf);
    http::response_header hdr;
    auto parse_code = hdr.parse(hdr_str).first;
    ctx.require_eq(parse_code, http::status::ok, loc);
    std::map<std::string, std::string> fields;
    hdr.for_each_field([&fields](auto key, auto value) { //
      fields.emplace(key, value);
    });
    return {static_cast<http::status>(hdr.status()), std::move(fields),
            byte_buffer{payload.begin(), payload.end()}};
  }

  std::string ascii(const byte_buffer& buf, std::source_location loc
                                            = std::source_location::current()) {
    auto& ctx = test::runnable::current();
    std::string str;
    str.reserve(buf.size());
    for (auto b : buf) {
      if (!std::isprint(static_cast<uint8_t>(b))) {
        ctx.fail({"non-ASCII character in payload", loc});
      }
      str.push_back(static_cast<char>(b));
    }
    return str;
  }

  net::multiplexer_ptr mpx;
  net::socket_guard<net::stream_socket> sguard1;
  net::socket_guard<net::stream_socket> sguard2;
  std::thread mpx_thread;
  std::vector<http::route_ptr> routes;
  disposable server;
};

} // namespace

WITH_FIXTURE(fixture) {

TEST("responding to a request with a status code only") {
  route("/test", [](http::responder& res) { res.respond(http::status::ok); });
  launch();
  send_request(http::method::get, "/test");
  auto [code, fields, payload] = receive_response();
  check_eq(code, http::status::ok);
  check_eq(fields.size(), 1u);
  check_eq(fields["Content-Length"], "0");
  check_eq(payload.size(), 0u);
}

TEST("responding to a request with a payload") {
  route("/test", [](http::responder& res) {
    res.respond(http::status::ok, "text/plain", "Hello, world!");
  });
  launch();
  send_request(http::method::get, "/test");
  auto hello = "Hello, world!"sv;
  auto [code, fields, payload] = receive_response();
  check_eq(code, http::status::ok);
  check_eq(fields.size(), 2u);
  check_eq(fields["Content-Length"], std::to_string(hello.size()));
  check_eq(fields["Content-Type"], "text/plain");
  check_eq(payload.size(), hello.size());
  check_eq(ascii(payload), hello);
}

TEST("responding with custom header fields without payload") {
  route("/test", [](http::responder& res) {
    res.begin_header(http::status::ok);
    res.add_header_field("Content-Length", "0");
    res.add_header_field("X-Custom-Header", "custom-value");
    res.end_header();
    res.send_payload(as_bytes(const_byte_span{}));
  });
  launch();
  send_request(http::method::get, "/test");
  auto [code, fields, payload] = receive_response();
  check_eq(code, http::status::ok);
  check_eq(fields.size(), 2u);
  check_eq(fields["Content-Length"], "0");
  check_eq(fields["X-Custom-Header"], "custom-value");
  check_eq(payload.size(), 0u);
}

TEST("responding with custom header fields with payload") {
  route("/test", [](http::responder& res) {
    auto hello = "Hello, world!"sv;
    auto bytes = as_bytes(std::span{hello});
    res.begin_header(http::status::ok);
    res.add_header_field("Content-Type", "text/plain");
    res.add_header_field("Content-Length", std::to_string(hello.size()));
    res.add_header_field("X-Custom-Header", "custom-value");
    res.end_header();
    res.send_payload(bytes);
  });
  launch();
  send_request(http::method::get, "/test");
  auto hello = "Hello, world!"sv;
  auto [code, fields, payload] = receive_response();
  check_eq(code, http::status::ok);
  check_eq(fields.size(), 3u);
  check_eq(fields["Content-Length"], std::to_string(hello.size()));
  check_eq(fields["Content-Type"], "text/plain");
  check_eq(fields["X-Custom-Header"], "custom-value");
  check_eq(payload.size(), hello.size());
  check_eq(ascii(payload), hello);
}

} // WITH_FIXTURE(fixture)
