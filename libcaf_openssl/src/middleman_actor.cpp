/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/io/middleman_actor.hpp"

#include <tuple>
#include <stdexcept>
#include <utility>

#include "caf/sec.hpp"
#include "caf/send.hpp"
#include "caf/actor.hpp"
#include "caf/logger.hpp"
#include "caf/node_id.hpp"
#include "caf/actor_proxy.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/typed_event_based_actor.hpp"

#include "caf/io/basp_broker.hpp"
#include "caf/io/system_messages.hpp"
#include "caf/io/middleman_actor_impl.hpp"

#include "caf/io/network/interfaces.hpp"
#include "caf/io/network/stream_impl.hpp"
#include "caf/io/network/doorman_impl.hpp"
#include "caf/io/network/default_multiplexer.hpp"

#include "caf/openssl/session.hpp"

#ifdef CAF_WINDOWS
# include <winsock2.h>
# include <ws2tcpip.h> // socket_size_type, etc. (MSVC20xx)
#else
# include <sys/types.h>
# include <sys/socket.h>
#endif

namespace caf {
namespace openssl {

namespace {

using native_socket = io::network::native_socket;
using default_mpx = io::network::default_multiplexer;

struct ssl_policy {
  ssl_policy(session_ptr session) : session_(std::move(session)) {
    // nop
  }

  rw_state read_some(size_t& result, native_socket fd, void* buf, size_t len) {
    CAF_LOG_TRACE(CAF_ARG(fd) << CAF_ARG(len));
    return session_->read_some(result, fd, buf, len);
  }

  rw_state write_some(size_t& result, native_socket fd, const void* buf,
                      size_t len) {
    CAF_LOG_TRACE(CAF_ARG(fd) << CAF_ARG(len));
    return session_->write_some(result, fd, buf, len);
  }

  bool try_accept(native_socket& result, native_socket fd) {
    CAF_LOG_TRACE(CAF_ARG(fd));
    sockaddr_storage addr;
    memset(&addr, 0, sizeof(addr));
    caf::io::network::socket_size_type addrlen = sizeof(addr);
    result = accept(fd, reinterpret_cast<sockaddr*>(&addr), &addrlen);
    // note accept4 is better to avoid races in setting CLOEXEC (but not posix)
    io::network::child_process_inherit(result, false);
    CAF_LOG_DEBUG(CAF_ARG(fd) << CAF_ARG(result));
    if (result == io::network::invalid_native_socket) {
      auto err = io::network::last_socket_error();
      if (!io::network::would_block_or_temporarily_unavailable(err))
        return false;
    }
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
    scribe_impl(default_mpx& mpx, native_socket sockfd,
                session_ptr sptr)
        : scribe(io::network::conn_hdl_from_socket(sockfd)),
          launched_(false),
          stream_(mpx, sockfd, std::move(sptr)) {
      // nop
    }

    ~scribe_impl() {
      CAF_LOG_TRACE("");
    }

    void configure_read(io::receive_policy::config config) override {
      CAF_LOG_TRACE(CAF_ARG(config));
      stream_.configure_read(config);
      if (!launched_)
        launch();
    }

    void ack_writes(bool enable) override {
      CAF_LOG_TRACE(CAF_ARG(enable));
      stream_.ack_writes(enable);
    }

    std::vector<char>& wr_buf() override {
      return stream_.wr_buf();
    }

    std::vector<char>& rd_buf() override {
      return stream_.rd_buf();
    }

    void graceful_shutdown() override {
      CAF_LOG_TRACE("");
      stream_.graceful_shutdown();
      detach(&stream_.backend(), false);
    }

    void flush() override {
      CAF_LOG_TRACE("");
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
      CAF_LOG_TRACE("");
      CAF_ASSERT(!launched_);
      launched_ = true;
      stream_.start(this);
      // This schedules the scribe in case SSL still needs to call SSL_connect
      // or SSL_accept. Otherwise, the backend simply removes the socket for
      // write operations after the first "nop write".
      stream_.force_empty_write(this);
    }

    void add_to_loop() override {
      CAF_LOG_TRACE("");
      stream_.activate(this);
    }

    void remove_from_loop() override {
      CAF_LOG_TRACE("");
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
    CAF_LOG_TRACE("");
    if (detached())
       // we are already disconnected from the broker while the multiplexer
       // did not yet remove the socket, this can happen if an I/O event causes
       // the broker to call close_all() while the pollset contained
       // further activities for the broker
       return false;
    auto& dm = acceptor_.backend();
    auto fd = acceptor_.accepted_socket();
    io::network::nonblocking(fd, true);
    auto sssn = make_session(parent()->system(), fd, true);
    if (sssn == nullptr) {
      CAF_LOG_ERROR("Unable to create SSL session for accepted socket");
      return false;
    }
    auto scrb = make_counted<scribe_impl>(dm, fd, std::move(sssn));
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
    CAF_LOG_TRACE(CAF_ARG(host) << CAF_ARG(port));
    auto fd = io::network::new_tcp_connection(host, port);
    if (!fd)
      return std::move(fd.error());
    io::network::nonblocking(*fd, true);
    auto sssn = make_session(system(), *fd, false);
    if (!sssn) {
      CAF_LOG_ERROR("Unable to create SSL session for connection");
      return sec::cannot_connect_to_node;
    }
    CAF_LOG_DEBUG("successfully created an SSL session for:"
                  << CAF_ARG(host) << CAF_ARG(port));
    return make_counted<scribe_impl>(mpx(), *fd, std::move(sssn));
  }

  expected<io::doorman_ptr> open(uint16_t port, const char* addr,
                                 bool reuse) override {
    CAF_LOG_TRACE(CAF_ARG(port) << CAF_ARG(reuse));
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

} // namespace <anonymous>

io::middleman_actor make_middleman_actor(actor_system& sys, actor db) {
  return !get_or(sys.config(), "middleman.attach-utility-actors", false)
         ? sys.spawn<middleman_actor_impl, detached + hidden>(std::move(db))
         : sys.spawn<middleman_actor_impl, hidden>(std::move(db));
}

} // namespace openssl
} // namespace caf
