/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
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
#include "caf/io/network/default_multiplexer.hpp"

namespace caf {
namespace openssl {

namespace {

using native_socket = io::network::native_socket;
using default_mpx = io::network::default_multiplexer;

struct ssl_policy {
  /// Reads up to `len` bytes from an OpenSSL socket.
  static bool read_some(size_t& result, native_socket fd, void* buf,
                        size_t len) {
    CAF_LOG_TRACE(CAF_ARG(fd) << CAF_ARG(len));
    // TODO: implement me
    CAF_RAISE_ERROR("unimplemented function: openssl::read_some");
  }

  static bool write_some(size_t& result, native_socket fd, const void* buf,
                         size_t len) {
    CAF_LOG_TRACE(CAF_ARG(fd) << CAF_ARG(len));
    // TODO: implement me
    CAF_RAISE_ERROR("unimplemented function: openssl::write_some");
  }

  static bool try_accept(native_socket& result, native_socket fd) {
    CAF_LOG_TRACE(CAF_ARG(fd));
    // TODO: implement me
    CAF_RAISE_ERROR("unimplemented function: openssl::try_accept");
  }
};

class scribe_impl : public io::scribe {
  public:
    scribe_impl(default_mpx& mpx, native_socket sockfd)
        : scribe(io::network::conn_hdl_from_socket(sockfd)),
          launched_(false),
          stream_(mpx, sockfd) {
      // nop
    }

    void configure_read(io::receive_policy::config config) override {
      CAF_LOG_TRACE("");
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

    void stop_reading() override {
      CAF_LOG_TRACE("");
      stream_.stop_reading();
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
    }

    void add_to_loop() override {
      stream_.activate(this);
    }

    void remove_from_loop() override {
      stream_.passivate();
    }

  private:
    bool launched_;
    io::network::stream_impl<ssl_policy> stream_;
};

class doorman_impl : public io::doorman {
public:
  doorman_impl(default_mpx& mx, native_socket sockfd)
      : doorman(io::network::accept_hdl_from_socket(sockfd)),
        acceptor_(mx, sockfd) {
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
    auto sptr = dm.new_scribe(acceptor_.accepted_socket());
    auto hdl = sptr->hdl();
    parent()->add_scribe(std::move(sptr));
    return doorman::new_connection(&dm, hdl);
  }

  void stop_reading() override {
    CAF_LOG_TRACE("");
    acceptor_.stop_reading();
    detach(&acceptor_.backend(), false);
  }

  void launch() override {
    CAF_LOG_TRACE("");
    acceptor_.start(this);
  }

  std::string addr() const override {
    auto x = io::network::local_addr_of_fd(acceptor_.fd());
    if (!x)
      return "";
    return std::move(*x);
  }

  uint16_t port() const override {
    auto x = io::network::local_port_of_fd(acceptor_.fd());
    if (!x)
      return 0;
    return *x;
  }

  void add_to_loop() override {
    acceptor_.activate(this);
  }

  void remove_from_loop() override {
    acceptor_.passivate();
  }

private:
  io::network::acceptor_impl<ssl_policy> acceptor_;
};

class middleman_actor_impl : public io::middleman_actor_impl {
public:
  middleman_actor_impl(actor_config& cfg, actor default_broker)
      : io::middleman_actor_impl(cfg, std::move(default_broker)) {
    // nop
  }

protected:
  expected<io::scribe_ptr> connect(const std::string& host,
                                   uint16_t port) override {
    native_socket fd;
    // TODO: implement me
    return make_counted<scribe_impl>(mpx(), fd);
  }

  expected<io::doorman_ptr> open(uint16_t port, const char* addr,
                                 bool reuse) override {
    native_socket fd;
    // TODO: implement me
    return make_counted<doorman_impl>(mpx(), fd);
  }

private:
  default_mpx& mpx() {
    return static_cast<default_mpx&>(system().middleman().backend());
  }
};

} // namespace <anonymous>

io::middleman_actor make_middleman_actor(actor_system& sys, actor db) {
  return sys.config().middleman_detach_utility_actors
             ? sys.spawn<middleman_actor_impl, detached + hidden>(std::move(db))
             : sys.spawn<middleman_actor_impl, hidden>(std::move(db));
}

} // namespace openssl
} // namespace caf
