// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE net.openssl_transport

#include "caf/net/openssl_transport.hpp"

#include "net-test.hpp"
#include "pem.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/net/stream_socket.hpp"

#include <filesystem>
#include <random>

using namespace caf;
using namespace caf::net;

namespace {

using byte_buffer_ptr = std::shared_ptr<byte_buffer>;

struct fixture {
  using byte_buffer_ptr = std::shared_ptr<byte_buffer>;

  fixture() {
    multiplexer::block_sigpipe();
    OPENSSL_init_ssl(OPENSSL_INIT_SSL_DEFAULT, nullptr);
    // Make a directory name with 8 random (hex) character suffix.
    std::string dir_name = "caf-net-test-";
    std::random_device rd;
    std::minstd_rand rng{rd()};
    std::uniform_int_distribution<int> dist(0, 255);
    for (int i = 0; i < 4; ++i)
      detail::append_hex(dir_name, static_cast<uint8_t>(dist(rng)));
    // Create the directory under /tmp (or its equivalent on non-POSIX).
    namespace fs = std::filesystem;
    tmp_dir = fs::temp_directory_path() / dir_name;
    if (!fs::create_directory(tmp_dir)) {
      std::cerr << "*** failed to create " << tmp_dir.string() << "\n";
      abort();
    }
    // Create the .pem files on disk.
    write_file("ca.pem", ca_pem);
    write_file("cert.1.pem", cert_1_pem);
    write_file("cert.2.pem", cert_1_pem);
    write_file("key.1.enc.pem", key_1_enc_pem);
    write_file("key.1.pem", key_1_pem);
    write_file("key.2.pem", key_1_pem);
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

  ~fixture() {
    // Clean up our files under /tmp.
    if (!tmp_dir.empty())
      std::filesystem::remove_all(tmp_dir);
  }

  std::string abs_path(std::string_view fname) {
    auto path = tmp_dir / fname;
    return path.string();
  }

  void write_file(std::string_view fname, std::string_view content) {
    std::ofstream out{abs_path(fname)};
    out << content;
  }

  std::filesystem::path tmp_dir;
};

class dummy_app {
public:
  using input_tag = tag::stream_oriented;

  explicit dummy_app(std::shared_ptr<bool> done, byte_buffer_ptr recv_buf)
    : done_(std::move(done)), recv_buf_(std::move(recv_buf)) {
    // nop
  }

  ~dummy_app() {
    *done_ = true;
  }

  template <class ParentPtr>
  error init(socket_manager*, ParentPtr parent, const settings&) {
    MESSAGE("initialize dummy app");
    parent->configure_read(receive_policy::exactly(4));
    parent->begin_output();
    auto& buf = parent->output_buffer();
    caf::binary_serializer out{nullptr, buf};
    static_cast<void>(out.apply(10));
    parent->end_output();
    return none;
  }

  template <class ParentPtr>
  bool prepare_send(ParentPtr) {
    return true;
  }

  template <class ParentPtr>
  bool done_sending(ParentPtr) {
    return true;
  }

  template <class ParentPtr>
  void continue_reading(ParentPtr) {
    // nop
  }

  template <class ParentPtr>
  size_t consume(ParentPtr down, const_byte_span data, const_byte_span) {
    MESSAGE("dummy app received " << data.size() << " bytes");
    // Store the received bytes.
    recv_buf_->insert(recv_buf_->begin(), data.begin(), data.end());
    // Respond with the same data and return.
    down->begin_output();
    auto& out = down->output_buffer();
    out.insert(out.end(), data.begin(), data.end());
    down->end_output();
    return static_cast<ptrdiff_t>(data.size());
  }

  template <class ParentPtr>
  void abort(ParentPtr, const error& reason) {
    MESSAGE("dummy_app::abort called: " << reason);
    *done_ = true;
  }

private:
  std::shared_ptr<bool> done_;
  byte_buffer_ptr recv_buf_;
};

// Simulates a remote SSL server.
void dummy_tls_server(stream_socket fd, std::string cert_file,
                      std::string key_file) {
  namespace ssl = caf::net::openssl;
  multiplexer::block_sigpipe();
  // Make sure we close our socket.
  auto guard = detail::make_scope_guard([fd] { close(fd); });
  // Get and configure our SSL context.
  auto ctx = ssl::make_ctx(TLS_server_method());
  if (auto err = ssl::certificate_pem_file(ctx, cert_file)) {
    std::cerr << "*** certificate_pem_file failed: " << ssl::fetch_error_str();
    return;
  }
  if (auto err = ssl::private_key_pem_file(ctx, key_file)) {
    std::cerr << "*** private_key_pem_file failed: " << ssl::fetch_error_str();
    return;
  }
  // Perform SSL handshake.
  auto f = net::openssl::policy::make(std::move(ctx), fd);
  if (f.accept(fd) <= 0) {
    std::cerr << "*** accept failed: " << ssl::fetch_error_str();
    return;
  }
  // Do some ping-pong messaging.
  for (int i = 0; i < 4; ++i) {
    byte_buffer buf;
    buf.resize(4);
    f.read(fd, buf);
    f.write(fd, buf);
  }
  // Graceful shutdown.
  f.notify_close();
}

// Simulates a remote SSL client.
void dummy_tls_client(stream_socket fd) {
  multiplexer::block_sigpipe();
  // Make sure we close our socket.
  auto guard = detail::make_scope_guard([fd] { close(fd); });
  // Perform SSL handshake.
  auto f = net::openssl::policy::make(TLS_client_method(), fd);
  if (f.connect(fd) <= 0) {
    ERR_print_errors_fp(stderr);
    return;
  }
  // Do some ping-pong messaging.
  for (int i = 0; i < 4; ++i) {
    byte_buffer buf;
    buf.resize(4);
    f.read(fd, buf);
    f.write(fd, buf);
  }
  // Graceful shutdown.
  f.notify_close();
}

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("openssl::async_connect performs the client handshake") {
  GIVEN("a connection to a TLS server") {
    auto [serv_fd, client_fd] = no_sigpipe(unbox(make_stream_socket_pair()));
    if (auto err = net::nonblocking(client_fd, true))
      FAIL("net::nonblocking failed: " << err);
    std::thread server{dummy_tls_server, serv_fd, abs_path("cert.1.pem"),
                       abs_path("key.1.pem")};
    WHEN("connecting as a client to an OpenSSL server") {
      THEN("openssl::async_connect transparently calls SSL_connect") {
        using stack_t = openssl_transport<dummy_app>;
        net::multiplexer mpx{nullptr};
        mpx.set_thread_id();
        auto done = std::make_shared<bool>(false);
        auto buf = std::make_shared<byte_buffer>();
        auto make_manager = [done, buf](stream_socket fd, multiplexer* ptr,
                                        net::openssl::policy policy) {
          return make_socket_manager<stack_t>(fd, ptr, std::move(policy), done,
                                              buf);
        };
        auto on_connect_error = [](const error& reason) {
          FAIL("connect failed: " << reason);
        };
        net::openssl::async_connect(client_fd, &mpx,
                                    net::openssl::policy::make(SSLv23_method(),
                                                               client_fd),
                                    make_manager, on_connect_error);
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

SCENARIO("openssl::async_accept performs the server handshake") {
  GIVEN("a socket that is connected to a client") {
    auto [serv_fd, client_fd] = no_sigpipe(unbox(make_stream_socket_pair()));
    if (auto err = net::nonblocking(serv_fd, true))
      FAIL("net::nonblocking failed: " << err);
    std::thread client{dummy_tls_client, client_fd};
    WHEN("acting as the OpenSSL server") {
      THEN("openssl::async_accept transparently calls SSL_accept") {
        using stack_t = openssl_transport<dummy_app>;
        net::multiplexer mpx{nullptr};
        mpx.set_thread_id();
        auto done = std::make_shared<bool>(false);
        auto buf = std::make_shared<byte_buffer>();
        auto make_manager = [done, buf](stream_socket fd, multiplexer* ptr,
                                        net::openssl::policy policy) {
          return make_socket_manager<stack_t>(fd, ptr, std::move(policy), done,
                                              buf);
        };
        auto on_accept_error = [](const error& reason) {
          FAIL("accept failed: " << reason);
        };
        auto ssl = net::openssl::policy::make(TLS_server_method(), serv_fd);
        if (auto err = ssl.certificate_pem_file(abs_path("cert.1.pem")))
          FAIL("certificate_pem_file failed: " << err);
        if (auto err = ssl.private_key_pem_file(abs_path("key.1.pem")))
          FAIL("privat_key_pem_file failed: " << err);
        net::openssl::async_accept(serv_fd, &mpx, std::move(ssl), make_manager,
                                   on_accept_error);

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
