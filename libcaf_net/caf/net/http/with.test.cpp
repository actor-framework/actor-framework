// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/http/with.hpp"

#include "caf/test/test.hpp"

#include "caf/net/middleman.hpp"
#include "caf/net/socket.hpp"
#include "caf/net/socket_guard.hpp"
#include "caf/net/tcp_accept_socket.hpp"
#include "caf/net/tcp_stream_socket.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/fwd.hpp"

#include <chrono>
#include <future>
#include <latch>
#include <thread>

using namespace caf;
using namespace std::literals;

namespace {

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

} // namespace

TEST("connection lifetime tracking with outstanding requests") {
  // Setup.
  caf::actor_system_config cfg;
  cfg.load<caf::net::middleman>();
  caf::actor_system sys{cfg};
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
  // Setup.
  caf::actor_system_config cfg;
  cfg.load<caf::net::middleman>();
  caf::actor_system sys{cfg};
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
  // Setup.
  caf::actor_system_config cfg;
  cfg.load<caf::net::middleman>();
  caf::actor_system sys{cfg};
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
  // Setup.
  caf::actor_system_config cfg;
  cfg.load<caf::net::middleman>();
  caf::actor_system sys{cfg};
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
