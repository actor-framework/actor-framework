// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/io/middleman_actor.hpp"

#ifdef CAF_WINDOWS
#  include <winsock2.h>
#  include <ws2tcpip.h> // socket_size_type, etc. (MSVC20xx)
#else
#  include <sys/socket.h>
#  include <sys/types.h>
#endif

#include "caf/io/basp_broker.hpp"
#include "caf/io/middleman_actor_impl.hpp"
#include "caf/io/network/default_multiplexer.hpp"
#include "caf/io/network/doorman_impl.hpp"
#include "caf/io/network/interfaces.hpp"
#include "caf/io/network/stream_impl.hpp"
#include "caf/io/system_messages.hpp"

#include "caf/actor.hpp"
#include "caf/actor_proxy.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/detail/socket_guard.hpp"
#include "caf/log/openssl.hpp"
#include "caf/log/system.hpp"
#include "caf/node_id.hpp"
#include "caf/sec.hpp"
#include "caf/send.hpp"
#include "caf/typed_event_based_actor.hpp"

#include <stdexcept>
#include <tuple>
#include <utility>

#ifdef CAF_WINDOWS
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif // CAF_WINDOWS
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif // NOMINMAX
#  ifdef CAF_MINGW
#    undef _WIN32_WINNT
#    undef WINVER
#    define _WIN32_WINNT WindowsVista
#    define WINVER WindowsVista
#    include <w32api.h>
#  endif // CAF_MINGW
#  include <windows.h>
#  include <winsock2.h>
#  include <ws2ipdef.h>
#  include <ws2tcpip.h>
#else
#  include <sys/socket.h>
#  include <sys/types.h>
#endif

#include "caf/openssl/session.hpp"

namespace caf::openssl {

namespace {

using native_socket = io::network::native_socket;
using default_mpx = io::network::default_multiplexer;

struct ssl_policy {
  ssl_policy(session_ptr session) : session_(std::move(session)) {
    // nop
  }

  rw_state read_some(size_t& result, native_socket fd, void* buf, size_t len) {
    auto exit_guard = log::openssl::trace("fd = {}, len = {}", fd, len);
    return session_->read_some(result, fd, buf, len);
  }

  rw_state write_some(size_t& result, native_socket fd, const void* buf,
                      size_t len) {
    auto exit_guard = log::openssl::trace("fd = {}, len = {}", fd, len);
    return session_->write_some(result, fd, buf, len);
  }

  bool try_accept(native_socket& result, native_socket fd) {
    auto exit_guard = log::openssl::trace("fd = {}", fd);
    sockaddr_storage addr;
    memset(&addr, 0, sizeof(addr));
    caf::io::network::socket_size_type addrlen = sizeof(addr);
    result = accept(fd, reinterpret_cast<sockaddr*>(&addr), &addrlen);
    // note accept4 is better to avoid races in setting CLOEXEC (but not posix)
    if (result == io::network::invalid_native_socket) {
      auto err = io::network::last_socket_error();
      if (!io::network::would_block_or_temporarily_unavailable(err))
        CAF_LOG_ERROR(
          "accept failed:" << io::network::socket_error_as_string(err));
      return false;
    }
    io::network::child_process_inherit(result, false);
    log::openssl::debug("fd = {}, result = {}", fd, result);
    return session_->try_accept(result);
  }

  bool must_read_more(native_socket fd, size_t threshold) {
    return session_->must_read_more(fd, threshold);
  }

private:
  session_ptr session_;
};

class scribe_impl : public io::scribe {
public:
  scribe_impl(default_mpx& mpx, native_socket sockfd, session_ptr sptr)
    : scribe(io::network::conn_hdl_from_socket(sockfd)),
      launched_(false),
      stream_(mpx, sockfd, std::move(sptr)) {
    // nop
  }

  ~scribe_impl() override {
    auto exit_guard = log::openssl::trace("");
  }

  void configure_read(io::receive_policy::config config) override {
    auto exit_guard = log::openssl::trace("config = {}", config);
    stream_.configure_read(config);
    if (!launched_)
      launch();
  }

  void ack_writes(bool enable) override {
    auto exit_guard = log::openssl::trace("enable = {}", enable);
    stream_.ack_writes(enable);
  }

  byte_buffer& wr_buf() override {
    return stream_.wr_buf();
  }

  byte_buffer& rd_buf() override {
    return stream_.rd_buf();
  }

  void graceful_shutdown() override {
    auto exit_guard = log::openssl::trace("");
    stream_.graceful_shutdown();
    detach(&stream_.backend(), false);
  }

  void flush() override {
    auto exit_guard = log::openssl::trace("");
    stream_.flush(this);
  }

  std::string addr() const override {
    auto x = io::network::remote_addr_of_fd(stream_.fd());
    if (!x)
      return "";
    return *x;
  }

  uint16_t port() const override {
    auto x = io::network::remote_port_of_fd(stream_.fd());
    if (!x)
      return 0;
    return *x;
  }

  void launch() {
    auto exit_guard = log::openssl::trace("");
    CAF_ASSERT(!launched_);
    launched_ = true;
    stream_.start(this);
    // This schedules the scribe in case SSL still needs to call SSL_connect
    // or SSL_accept. Otherwise, the backend simply removes the socket for
    // write operations after the first "nop write".
    stream_.force_empty_write(this);
  }

  void add_to_loop() override {
    auto exit_guard = log::openssl::trace("");
    stream_.activate(this);
  }

  void remove_from_loop() override {
    auto exit_guard = log::openssl::trace("");
    stream_.passivate();
  }

private:
  bool launched_;
  io::network::stream_impl<ssl_policy> stream_;
};

class doorman_impl : public io::network::doorman_impl {
public:
  doorman_impl(default_mpx& mx, native_socket sockfd)
    : io::network::doorman_impl(mx, sockfd) {
    // nop
  }

  bool new_connection() override {
    auto exit_guard = log::openssl::trace("");
    if (detached())
      // we are already disconnected from the broker while the multiplexer
      // did not yet remove the socket, this can happen if an I/O event causes
      // the broker to call close_all() while the pollset contained
      // further activities for the broker
      return false;
    auto& dm = acceptor_.backend();
    auto fd = acceptor_.accepted_socket();
    detail::socket_guard sguard{fd};
    io::network::nonblocking(fd, true);
    auto sssn = make_session(parent()->system(), fd, true);
    if (sssn == nullptr) {
      log::system::error("unable to create SSL session for accepted socket");
      return false;
    }
    auto scrb = make_counted<scribe_impl>(dm, fd, std::move(sssn));
    sguard.release(); // The scribe claims ownership of the socket.
    auto hdl = scrb->hdl();
    parent()->add_scribe(std::move(scrb));
    return doorman::new_connection(&dm, hdl);
  }
};

class middleman_actor_impl : public io::middleman_actor_impl {
public:
  middleman_actor_impl(actor_config& cfg, actor default_broker)
    : io::middleman_actor_impl(cfg, std::move(default_broker)) {
    // nop
  }

  const char* name() const override {
    return "openssl::middleman_actor";
  }

protected:
  expected<io::scribe_ptr> connect(const std::string& host,
                                   uint16_t port) override {
    auto exit_guard = log::openssl::trace("host = {}, port = {}", host, port);
    auto fd = io::network::new_tcp_connection(host, port);
    if (!fd)
      return std::move(fd.error());
    io::network::nonblocking(*fd, true);
    auto sssn = make_session(system(), *fd, false);
    if (!sssn) {
      log::system::error("unable to create SSL session for connection");
      return sec::cannot_connect_to_node;
    }
    log::openssl::debug(
      "successfully created an SSL session for: host = {}, port = {}", host,
      port);
    return make_counted<scribe_impl>(mpx(), *fd, std::move(sssn));
  }

  expected<io::doorman_ptr> open(uint16_t port, const char* addr,
                                 bool reuse) override {
    auto exit_guard = log::openssl::trace("port = {}, reuse = {}", port, reuse);
    auto fd = io::network::new_tcp_acceptor_impl(port, addr, reuse);
    if (!fd)
      return std::move(fd.error());
    return make_counted<doorman_impl>(mpx(), *fd);
  }

private:
  default_mpx& mpx() {
    return static_cast<default_mpx&>(system().middleman().backend());
  }
};

} // namespace

io::middleman_actor make_middleman_actor(actor_system& sys, actor db) {
  return !get_or(sys.config(), "caf.middleman.attach-utility-actors", false)
           ? sys.spawn<middleman_actor_impl, detached + hidden>(std::move(db))
           : sys.spawn<middleman_actor_impl, hidden>(std::move(db));
}

} // namespace caf::openssl
