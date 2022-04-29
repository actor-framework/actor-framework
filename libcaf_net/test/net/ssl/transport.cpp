// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE net.ssl.transport

#include "caf/net/ssl/transport.hpp"

#include "net-test.hpp"
#include "pem.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/net/ssl/context.hpp"
#include "caf/net/stream_socket.hpp"

using namespace caf;
using namespace caf::net;

namespace {

using byte_buffer_ptr = std::shared_ptr<byte_buffer>;

struct fixture {
  using byte_buffer_ptr = std::shared_ptr<byte_buffer>;

  fixture() {
    multiplexer::block_sigpipe();
  }

  template <class SocketPair>
  SocketPair no_sigpipe(SocketPair pair) {
    auto [first, second] = pair;
    if (auto err = allow_sigpipe(first, false))
      FAIL("allow_sigpipe failed: " << err);
    if (auto err = allow_sigpipe(second, false))
      FAIL("allow_sigpipe failed: " << err);
    return pair;
  }
};

class mock_application : public net::stream_oriented::upper_layer {
public:
  explicit mock_application(std::shared_ptr<bool> done,
                            byte_buffer_ptr recv_buf)
    : done_(std::move(done)), recv_buf_(std::move(recv_buf)) {
    // nop
  }

  static auto make(std::shared_ptr<bool> done, byte_buffer_ptr recv_buf) {
    return std::make_unique<mock_application>(std::move(done),
                                              std::move(recv_buf));
  }

  ~mock_application() {
    *done_ = true;
  }

  error init(socket_manager*, net::stream_oriented::lower_layer* down,
             const settings&) override {
    MESSAGE("initialize dummy app");
    down_ = down;
    down->configure_read(receive_policy::exactly(4));
    down->begin_output();
    auto& buf = down->output_buffer();
    caf::binary_serializer out{nullptr, buf};
    static_cast<void>(out.apply(int32_t{10}));
    down->end_output();
    return none;
  }

  bool prepare_send() override {
    return true;
  }

  bool done_sending() override {
    return true;
  }

  ptrdiff_t consume(byte_span data, byte_span) override {
    MESSAGE("dummy app received " << data.size() << " bytes");
    // Store the received bytes.
    recv_buf_->insert(recv_buf_->begin(), data.begin(), data.end());
    // Respond with the same data and return.
    down_->begin_output();
    auto& out = down_->output_buffer();
    out.insert(out.end(), data.begin(), data.end());
    down_->end_output();
    return static_cast<ptrdiff_t>(data.size());
  }

  void abort(const error& reason) override {
    MESSAGE("dummy_app::abort called: " << reason);
    *done_ = true;
  }

private:
  net::stream_oriented::lower_layer* down_ = nullptr;
  std::shared_ptr<bool> done_;
  byte_buffer_ptr recv_buf_;
};

// Simulates a remote SSL server.
void dummy_tls_server(stream_socket fd, const char* cert_file,
                      const char* key_file) {
  namespace ssl = caf::net::ssl;
  multiplexer::block_sigpipe();
  // Make sure we close our socket.
  auto guard = detail::make_scope_guard([fd] { close(fd); });
  // Get and configure our SSL context.
  auto ctx = unbox(ssl::context::make_server(ssl::tls::any));
  if (!ctx.use_certificate_from_file(cert_file, ssl::format::pem)) {
    std::cerr << "*** failed to load certificate_file: "
              << ctx.last_error_string() << '\n';
    return;
  }
  if (!ctx.use_private_key_from_file(key_file, ssl::format::pem)) {
    std::cerr << "*** failed to load private key file: "
              << ctx.last_error_string() << '\n';
    return;
  }
  // Perform SSL handshake.
  auto conn = unbox(ctx.new_connection(fd));
  if (auto ret = conn.accept(); ret <= 0) {
    std::cerr << "*** accept failed: " << conn.last_error_string(ret) << '\n';
    return;
  }
  // Do some ping-pong messaging.
  for (int i = 0; i < 4; ++i) {
    byte_buffer buf;
    buf.resize(4);
    if (auto ret = conn.read(buf); ret <= 0) {
      std::cerr << "*** read failed: " << conn.last_error_string(ret) << '\n';
      return;
    }
    if (auto ret = conn.write(buf); ret <= 0) {
      std::cerr << "*** write failed: " << conn.last_error_string(ret) << '\n';
      return;
    }
  }
  // Graceful shutdown.
  conn.close();
}

// Simulates a remote SSL client.
void dummy_tls_client(stream_socket fd) {
  multiplexer::block_sigpipe();
  // Make sure we close our socket.
  auto guard = detail::make_scope_guard([fd] { close(fd); });
  // Perform SSL handshake.
  auto ctx = unbox(ssl::context::make_client(ssl::tls::any));
  auto conn = unbox(ctx.new_connection(fd));
  if (auto ret = conn.connect(); ret <= 0) {
    std::cerr << "*** connect failed: " << conn.last_error_string(ret) << '\n';
    return;
  }
  // Do some ping-pong messaging.
  for (int i = 0; i < 4; ++i) {
    byte_buffer buf;
    buf.resize(4);
    if (auto ret = conn.read(buf); ret <= 0) {
      std::cerr << "*** read failed: " << conn.last_error_string(ret) << '\n';
      return;
    }
    if (auto ret = conn.write(buf); ret <= 0) {
      std::cerr << "*** write failed: " << conn.last_error_string(ret) << '\n';
      return;
    }
  }
  // Graceful shutdown.
  conn.close();
}

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("ssl::transport::make_client performs the client handshake") {
  GIVEN("a connection to a TLS server") {
    auto [server_fd, client_fd] = no_sigpipe(unbox(make_stream_socket_pair()));
    if (auto err = net::nonblocking(client_fd, true))
      FAIL("net::nonblocking failed: " << err);
    std::thread server{dummy_tls_server, server_fd, cert_1_pem_path,
                       key_1_pem_path};
    WHEN("connecting as a client to an SSL server") {
      THEN("CAF transparently calls SSL_connect") {
        net::multiplexer mpx{nullptr};
        mpx.set_thread_id();
        std::ignore = mpx.init();
        auto ctx = unbox(ssl::context::make_client(ssl::tls::any));
        auto conn = unbox(ctx.new_connection(client_fd));
        auto done = std::make_shared<bool>(false);
        auto buf = std::make_shared<byte_buffer>();
        auto mock = mock_application::make(done, buf);
        auto transport = ssl::transport::make_client(std::move(conn),
                                                     std::move(mock));
        auto mgr = net::socket_manager::make(&mpx, client_fd,
                                             std::move(transport));
        mpx.init(mgr);
        mpx.apply_updates();
        while (!*done)
          mpx.poll_once(true);
        if (CHECK_EQ(buf->size(), 16u)) { // 4x 32-bit integers
          caf::binary_deserializer src{nullptr, *buf};
          for (int i = 0; i < 4; ++i) {
            int32_t value = 0;
            static_cast<void>(src.apply(value));
            CHECK_EQ(value, 10);
          }
        }
      }
    }
    server.join();
  }
}

SCENARIO("ssl::transport::make_server performs the server handshake") {
  GIVEN("a socket that is connected to a client") {
    auto [server_fd, client_fd] = no_sigpipe(unbox(make_stream_socket_pair()));
    if (auto err = net::nonblocking(server_fd, true))
      FAIL("net::nonblocking failed: " << err);
    std::thread client{dummy_tls_client, client_fd};
    WHEN("acting as the SSL server") {
      THEN("CAF transparently calls SSL_accept") {
        net::multiplexer mpx{nullptr};
        mpx.set_thread_id();
        std::ignore = mpx.init();
        auto ctx = unbox(ssl::context::make_server(ssl::tls::any));
        REQUIRE(ctx.use_certificate_from_file(cert_1_pem_path, //
                                              ssl::format::pem));
        REQUIRE(ctx.use_private_key_from_file(key_1_pem_path, //
                                              ssl::format::pem));
        auto conn = unbox(ctx.new_connection(server_fd));
        auto done = std::make_shared<bool>(false);
        auto buf = std::make_shared<byte_buffer>();
        auto mock = mock_application::make(done, buf);
        auto transport = ssl::transport::make_server(std::move(conn),
                                                     std::move(mock));
        auto mgr = net::socket_manager::make(&mpx, server_fd,
                                             std::move(transport));
        mpx.init(mgr);
        mpx.apply_updates();
        while (!*done)
          mpx.poll_once(true);
        if (CHECK_EQ(buf->size(), 16u)) { // 4x 32-bit integers
          caf::binary_deserializer src{nullptr, *buf};
          for (int i = 0; i < 4; ++i) {
            int32_t value = 0;
            static_cast<void>(src.apply(value));
            CHECK_EQ(value, 10);
          }
        }
      }
    }
    client.join();
  }
}

END_FIXTURE_SCOPE()
