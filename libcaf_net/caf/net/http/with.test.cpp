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
#include "caf/detail/latch.hpp"
#include "caf/fwd.hpp"

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

using latch_ptr = std::shared_ptr<detail::latch>;

void test_client(const char* host, uint16_t port, bool wait_before_connect,
                 latch_ptr lptr, error_log_ptr elog_ptr) {
  auto do_wait = [=] {
    if (!lptr->count_down_and_wait_for(1s)) {
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
  auto latch = std::make_shared<detail::latch>(4);
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
