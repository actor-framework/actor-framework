// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/http/with.hpp"

#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/net/http/response_header.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/socket.hpp"
#include "caf/net/socket_guard.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/net/tcp_accept_socket.hpp"
#include "caf/net/tcp_stream_socket.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/async/blocking_consumer.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/connector.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/fwd.hpp"
#include "caf/uri.hpp"

#include <chrono>
#include <future>
#include <latch>
#include <source_location>
#include <thread>

using namespace caf;
using namespace std::literals;

namespace {

/// A connector that returns a pre-existing socket on the first connect call.
/// Intended for unit tests that use connected socket pairs.
class socket_connector : public detail::connector {
public:
  explicit socket_connector(net::stream_socket fd) : fd_(fd) {
  }

  expected<net::stream_socket> connect(const std::string&, uint16_t, timespan,
                                       size_t, timespan) override {
    auto result = fd_;
    fd_.id = net::invalid_socket_id;
    return result;
  }

private:
  net::stream_socket fd_;
};

class error_log {
public:
  void append(error err) {
    std::lock_guard<std::mutex> guard{mtx_};
    errors_.push_back(std::move(err));
  }

  template <class Code>
  void append(Code code, std::string what) {
    append(make_error(code, std::move(what)));
  }

  auto errors() const {
    std::lock_guard<std::mutex> guard{mtx_};
    return errors_;
  }

private:
  mutable std::mutex mtx_;
  std::vector<error> errors_;
};

using error_log_ptr = std::shared_ptr<error_log>;

using latch_ptr = std::shared_ptr<std::latch>;

struct raw_http_response {
  net::http::response_header header;
  std::string body;
};

raw_http_response read_http_response(net::stream_socket fd,
                                     const std::source_location& loc
                                     = std::source_location::current()) {
  auto& self = test::runnable::current();
  auto require_readable = [&self, fd, loc] {
    auto err = net::receive_timeout(fd, 1s);
    self.require(!err.valid(), loc);
  };
  std::string raw_header;
  byte_buffer buf;
  buf.resize(1);
  while (raw_header.find("\r\n\r\n") == std::string::npos) {
    require_readable();
    auto bytes_read = net::read(fd, buf);
    self.require_eq(bytes_read, ptrdiff_t{1}, loc);
    raw_header.push_back(
      static_cast<char>(std::to_integer<unsigned char>(buf[0])));
  }
  raw_http_response result;
  auto parse_result = result.header.parse(raw_header);
  self.require_eq(parse_result.first, net::http::status::ok, loc);
  auto content_length = result.header.content_length().value_or(size_t{0});
  result.body.reserve(content_length);
  while (result.body.size() < content_length) {
    require_readable();
    buf.resize(content_length - result.body.size());
    auto bytes_read = net::read(fd, buf);
    self.require_gt(bytes_read, ptrdiff_t{0}, loc);
    result.body.append(reinterpret_cast<const char*>(buf.data()),
                       static_cast<size_t>(bytes_read));
  }
  return result;
}

// std::latch has no timed wait; poll try_wait() until timeout.
static bool latch_count_down_and_wait_for(latch_ptr lptr,
                                          std::chrono::seconds timeout) {
  lptr->count_down();
  auto deadline = std::chrono::steady_clock::now() + timeout;
  while (!lptr->try_wait()) {
    if (std::chrono::steady_clock::now() >= deadline)
      return false;
    std::this_thread::sleep_for(1ms);
  }
  return true;
}

void test_client(const char* host, uint16_t port, bool wait_before_connect,
                 latch_ptr lptr, error_log_ptr elog_ptr) {
  auto do_wait = [=] {
    if (!latch_count_down_and_wait_for(lptr, 1s)) {
      elog_ptr->append(sec::request_timeout,
                       "timeout while waiting on the latch");
      return;
    }
  };
  // Two workers will wait here. Meaning they will try to connect only after the
  // other two are connected.
  if (wait_before_connect) {
    do_wait();
  }
  auto maybe_fd = net::make_connected_tcp_stream_socket(host, port, 1s);
  if (!maybe_fd) {
    elog_ptr->append(maybe_fd.error());
    return;
  }
  auto fd = *maybe_fd;
  net::socket_guard guard{fd};
  // Two workers will wait here after connecting to delay when the other two
  // threads are trying to connect.
  if (!wait_before_connect) {
    do_wait();
  }
  // This sleep is to keep the connection open for a while in order to trigger
  // the max-connections limit.
  std::this_thread::sleep_for(50ms);
  // Send the HTTP request.
  auto request = detail::format("GET /status HTTP/1.1\r\n"
                                "Host: localhost:{}\r\n"
                                "User-Agent: AwesomeLib/1.0\r\n\r\n",
                                port);
  if (auto res = net::write(fd, as_bytes(std::span{request}));
      res != static_cast<ptrdiff_t>(request.size())) {
    elog_ptr->append(sec::socket_operation_failed,
                     "failed to send HTTP request");
    return;
  }
  // We don't really care about the response. Just read the first 10 bytes.
  if (auto err = net::receive_timeout(fd, 1s); err.valid()) {
    elog_ptr->append(err);
    return;
  }
  byte_buffer buf;
  buf.resize(10);
  if (auto res = net::read(fd, buf); res != 10) {
    auto msg = detail::format("failed to read HTTP response: {}",
                              net::last_socket_error_as_string());
    elog_ptr->append(sec::socket_operation_failed, std::move(msg));
  }
}

actor_system_config& init(actor_system_config& cfg) {
  cfg.set("caf.scheduler.max-threads", 2);
  cfg.load<net::middleman>();
  return cfg;
}

struct fixture {
  actor_system_config cfg;
  actor_system sys;

  fixture() : sys(init(cfg)) {
    // nop
  }
};

} // namespace

WITH_FIXTURE(fixture) {

TEST("connection lifetime tracking with outstanding requests") {
  // Create an accept socket with the port chosen by the OS.
  auto acceptor = unbox(net::make_tcp_accept_socket(0));
  auto port = unbox(net::local_port(acceptor));
  auto host = net::is_ipv4(acceptor) ? "127.0.0.1" : "::1";
  // Promise to capture the first request.
  auto prom = std::make_shared<std::promise<net::http::request>>();
  auto fut = std::optional{prom->get_future()};
  // Launch our server with max_connections = 1.
  auto hdl = net::http::with(sys)
               .accept(acceptor)
               .max_connections(1)
               .route("/test", net::http::method::get,
                      [prom](net::http::responder& res) mutable {
                        // Store the first request in the promise and respond
                        // with 200 OK for subsequent requests.
                        if (prom) {
                          prom->set_value(std::move(res).to_request());
                          prom.reset();
                          return;
                        }
                        res.respond(net::http::status::ok);
                      })
               .start();
  require_has_value(hdl);
  detail::scope_guard hdl_guard{[hdl]() mutable noexcept { hdl->dispose(); }};
  // Helper to send a request and wait for response with timeout.
  auto send_and_receive = [host, port](auto rel_timeout,
                                       bool wait_for_response) {
    auto maybe_fd = net::make_connected_tcp_stream_socket(host, port,
                                                          rel_timeout);
    if (!maybe_fd)
      return false;
    auto fd = *maybe_fd;
    net::socket_guard guard{fd};
    auto request = detail::format("GET /test HTTP/1.1\r\n"
                                  "Host: localhost:{}\r\n\r\n",
                                  port);
    if (net::write(fd, as_bytes(std::span{request}))
        != static_cast<ptrdiff_t>(request.size()))
      return false;
    if (wait_for_response) {
      if (auto err = net::receive_timeout(fd, rel_timeout); err.valid())
        return false;
      byte_buffer buf;
      buf.resize(100);
      return net::read(fd, buf) > 0;
    }
    return true;
  };
  // Connect and send, then close the socket without waiting for a response.
  check(send_and_receive(200ms, false));
  require(fut->wait_for(1s) == std::future_status::ready);
  // Try a second client while the request from the first client is still
  // around. The TCP connection might succeed (kernel backlog), but the HTTP
  // request should timeout because max_connections = 1.
  {
    auto req = fut->get();
    fut = std::nullopt;
    check(!send_and_receive(200ms, true));
    req.respond(net::http::status::ok, "text/plain", "done");
  }
  // No active connection left, so a new connection can be established again.
  check(send_and_receive(200ms, true));
}

TEST("requests become orphaned when the connection is closed") {
  // Create an accept socket with the port chosen by the OS.
  auto acceptor = unbox(net::make_tcp_accept_socket(0));
  auto port = unbox(net::local_port(acceptor));
  auto host = net::is_ipv4(acceptor) ? "127.0.0.1" : "::1";
  // Promise to capture the first request.
  auto prom = std::make_shared<std::promise<net::http::request>>();
  auto fut = std::optional{prom->get_future()};
  // Launch our server with max_connections = 1.
  auto hdl = net::http::with(sys)
               .accept(acceptor)
               .max_connections(1)
               .route("/test", net::http::method::get,
                      [prom](net::http::responder& res) mutable {
                        // Store the first request in the promise and respond
                        // with 200 OK for subsequent requests.
                        if (prom) {
                          prom->set_value(std::move(res).to_request());
                          prom.reset();
                          return;
                        }
                        res.respond(net::http::status::ok);
                      })
               .start();
  require_has_value(hdl);
  detail::scope_guard hdl_guard{[hdl]() mutable noexcept { hdl->dispose(); }};
  // Helper to send a request and wait for response with timeout.
  auto send_and_receive = [host, port](auto rel_timeout,
                                       bool wait_for_response) {
    auto maybe_fd = net::make_connected_tcp_stream_socket(host, port,
                                                          rel_timeout);
    if (!maybe_fd)
      return false;
    auto fd = *maybe_fd;
    net::socket_guard guard{fd};
    auto request = detail::format("GET /test HTTP/1.1\r\n"
                                  "Host: localhost:{}\r\n\r\n",
                                  port);
    if (net::write(fd, as_bytes(std::span{request}))
        != static_cast<ptrdiff_t>(request.size()))
      return false;
    if (wait_for_response) {
      if (auto err = net::receive_timeout(fd, rel_timeout); err.valid())
        return false;
      byte_buffer buf;
      buf.resize(100);
      return net::read(fd, buf) > 0;
    }
    return true;
  };
  // Connect and send, then close the socket without waiting for a response.
  check(send_and_receive(200ms, false));
  require(fut->wait_for(1s) == std::future_status::ready);
  // The request should become orphaned since the client closed the socket.
  auto req = fut->get();
  auto start = std::chrono::steady_clock::now();
  while (!req.orphaned() && std::chrono::steady_clock::now() - start < 1s) {
    std::this_thread::sleep_for(20ms);
  }
  check(req.orphaned());
}

TEST("requests become orphaned when disposing the server") {
  // Create an accept socket with the port chosen by the OS.
  auto acceptor = unbox(net::make_tcp_accept_socket(0));
  auto port = unbox(net::local_port(acceptor));
  auto host = net::is_ipv4(acceptor) ? "127.0.0.1" : "::1";
  // Promise to capture the first request.
  auto prom = std::make_shared<std::promise<net::http::request>>();
  auto fut = std::optional{prom->get_future()};
  // Launch our server with max_connections = 1.
  auto hdl = net::http::with(sys)
               .accept(acceptor)
               .max_connections(1)
               .route("/test", net::http::method::get,
                      [prom](net::http::responder& res) mutable {
                        // Store the first request in the promise and respond
                        // with 200 OK for subsequent requests.
                        if (prom) {
                          prom->set_value(std::move(res).to_request());
                          prom.reset();
                          return;
                        }
                        res.respond(net::http::status::ok);
                      })
               .start();
  require_has_value(hdl);
  detail::scope_guard hdl_guard{[hdl]() mutable noexcept { hdl->dispose(); }};
  // Connect to the server.
  auto maybe_fd = net::make_connected_tcp_stream_socket(host, port, 200ms);
  require_has_value(maybe_fd);
  auto fd = *maybe_fd;
  net::socket_guard guard{fd};
  auto request = detail::format("GET /test HTTP/1.1\r\n"
                                "Host: localhost:{}\r\n\r\n",
                                port);
  require_eq(net::write(fd, as_bytes(std::span{request})),
             static_cast<ptrdiff_t>(request.size()));
  // The server should emit the request now.
  require(fut->wait_for(1s) == std::future_status::ready);
  // Disposing the server should cause the request to become orphaned.
  auto req = fut->get();
  hdl->dispose();
  auto start = std::chrono::steady_clock::now();
  while (!req.orphaned() && std::chrono::steady_clock::now() - start < 1s) {
    std::this_thread::sleep_for(20ms);
  }
  check(req.orphaned());
}

TEST("GH-2226 regression") {
  // Create an accept socket with the port chosen by the OS.
  auto acceptor = unbox(net::make_tcp_accept_socket(0));
  auto port = unbox(net::local_port(acceptor));
  auto host = net::is_ipv4(acceptor) ? "127.0.0.1" : "::1";
  // Launch our server.
  auto hdl = net::http::with(sys)
               .accept(acceptor)
               .max_connections(2)
               .route("/status", net::http::method::get,
                      [](net::http::responder& res) {
                        res.respond(net::http::status::no_content);
                      })
               .start();
  require_has_value(hdl);
  // Launch our four clients.
  auto latch = std::make_shared<std::latch>(4);
  auto elog = std::make_shared<error_log>();
  std::thread clients[4];
  // Two clients connect right away, the other two wait on the latch until the
  // first two connections have been established.
  clients[0] = std::thread(test_client, host, port, true, latch, elog);
  clients[1] = std::thread(test_client, host, port, true, latch, elog);
  clients[2] = std::thread(test_client, host, port, false, latch, elog);
  clients[3] = std::thread(test_client, host, port, false, latch, elog);
  // Wait for all clients. At most they will run for about 1 second due to the
  // timeouts in the client code.
  for (auto& client : clients) {
    client.join();
  }
  // Wrap up and print client errors, if any.
  hdl->dispose();
  check_eq(elog->errors(), std::vector<error>{});
}

SCENARIO("server responds with 503 unavailable when queue is overloaded") {
  GIVEN("a HTTP server queueing incoming requests asynchronously") {
    auto overflow_count = size_t{2};
    auto [server_fd, client_fd] = unbox(net::make_stream_socket_pair());
    net::socket_guard client_guard{client_fd};
    std::optional<async::consumer_resource<net::http::request>> requests;
    auto hdl
      = net::http::with(sys) //
          .serve(server_fd)
          .start([&requests](auto pull) { requests.emplace(std::move(pull)); });
    require_has_value(hdl);
    detail::scope_guard hdl_guard{[hdl]() mutable noexcept { hdl->dispose(); }};
    WHEN("filling the queue capacity and overflowing by two requests") {
      // When configuring HTTP with a stream socket, all requests are
      // exchanged via the connected socket pair. This might change with
      // future connection keepalive changes.
      auto request_count = defaults::flow::buffer_size + overflow_count;
      // Fill the queue, and send two requests that overflow the queue.
      for (auto i = size_t{0}; i < request_count; ++i) {
        auto req = detail::format("GET /{} HTTP/1.1\r\nHost: localhost\r\n\r\n",
                                  i);
        require_eq(net::write(client_fd, as_bytes(std::span{req})),
                   static_cast<ptrdiff_t>(req.size()));
      }
      THEN("the two overflow requests receive 503 responses") {
        for (auto i = size_t{0}; i < overflow_count; ++i) {
          auto response = read_http_response(client_fd);
          check_eq(response.header.status(), uint16_t{503});
          check_eq(response.header.status_text(), "Service Unavailable");
          check_eq(response.body,
                   "service unavailable: request could not be queued");
        }
      }
      AND_THEN("the remaining requests are processed in order") {
        // Respond 200 to each earlier request.
        require(requests.has_value());
        auto request_buffer = requests->try_open();
        require(request_buffer != nullptr);
        async::blocking_consumer<net::http::request> consumer{request_buffer};
        for (auto i = size_t{0}; i < defaults::flow::buffer_size; ++i) {
          net::http::request req;
          auto result = consumer.pull(async::delay_errors, req, 1s);
          require_eq(result, async::read_result::ok);
          req.respond(net::http::status::ok, "text/plain", "ok");
        }
        request_buffer->close();
        net::http::request dummy;
        require_eq(consumer.pull(async::delay_errors, dummy, 1s),
                   async::read_result::stop);
        for (auto i = size_t{0}; i < defaults::flow::buffer_size; ++i) {
          auto response = read_http_response(client_fd);
          check_eq(response.header.status(), uint16_t{200});
          check_eq(response.body, "ok");
        }
      }
    }
  }
}

TEST("GH-2309 Host header field in outgoing requests") {
  auto parse_uri = [this](std::string_view sv) {
    uri u;
    auto err = parse(sv, u);
    require(err.empty());
    return u;
  };
  auto [server_fd, client_fd] = unbox(net::make_stream_socket_pair());
  std::promise<std::string> prom;
  auto host_fut = prom.get_future();
  auto hdl
    = net::http::with(sys)
        .serve(server_fd)
        .route("/host-check", net::http::method::get,
               [prom = std::move(prom)](net::http::responder& res) mutable {
                 prom.set_value(std::string{res.header().field("Host")});
                 res.respond(net::http::status::ok, "text/plain", "");
               })
        .start();
  require_has_value(hdl);
  detail::scope_guard hdl_guard{[hdl]() mutable noexcept { hdl->dispose(); }};
  auto assert_request_host_is = [this, &host_fut,
                                 client_fd](const caf::uri& uri,
                                            std::string_view expected_host) {
    auto client_res
      = net::http::with(sys)
          .connect(uri)
          .connector(std::make_unique<socket_connector>(client_fd))
          .connection_timeout(1s)
          .max_retry_count(1)
          .get();
    require_has_value(client_res);
    auto maybe_resp = client_res->first.get(1s);
    require_has_value(maybe_resp);
    require(host_fut.wait_for(1s) == std::future_status::ready);
    check_eq(host_fut.get(), expected_host);
  };

  SECTION("http IPv4 with non-default port") {
    assert_request_host_is(parse_uri("http://127.0.0.1:8080/host-check"),
                           "127.0.0.1:8080");
  }
  SECTION("http IPv4 omitted port") {
    assert_request_host_is(parse_uri("http://127.0.0.1/host-check"),
                           "127.0.0.1");
  }
  SECTION("http IPv4 explicit default port") {
    assert_request_host_is(parse_uri("http://127.0.0.1:80/host-check"),
                           "127.0.0.1");
  }
  SECTION("http hostname with non-default port") {
    assert_request_host_is(parse_uri("http://api.example.org:3000/host-check"),
                           "api.example.org:3000");
  }
  SECTION("http IPv6 with non-default port") {
    assert_request_host_is(parse_uri("http://[::1]:9090/host-check"),
                           "[::1]:9090");
  }
  SECTION("http IPv6 omitted port") {
    assert_request_host_is(parse_uri("http://[::1]/host-check"), "[::1]");
  }
  SECTION("http IPv6 explicit default port") {
    assert_request_host_is(parse_uri("http://[::1]:80/host-check"), "[::1]");
  }
}

} // WITH_FIXTURE(fixture)
