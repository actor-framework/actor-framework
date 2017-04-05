/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 * Raphael Hiesgen <raphael.hiesgen (at) haw-hamburg.de>                      *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/io/broker.hpp"
#include "caf/io/middleman.hpp"

#include "caf/scheduler/abstract_coordinator.hpp"

#include "caf/io/network/asio_multiplexer.hpp"

namespace caf {
namespace io {
namespace network {

namespace {

/// A wrapper for the supervisor backend provided by boost::asio.
struct asio_supervisor : public multiplexer::supervisor {
  explicit asio_supervisor(io_service& iob) : work(iob) {
    // nop
  }

private:
  boost::asio::io_service::work work;
};

} // namespace anonymous

expected<asio_tcp_socket>
new_tcp_connection(io_service& ios, const std::string& host, uint16_t port) {
  asio_tcp_socket fd{ios};
  using boost::asio::ip::tcp;
  tcp::resolver r(fd.get_io_service());
  tcp::resolver::query q(host, std::to_string(port));
  boost::system::error_code ec;
  auto i = r.resolve(q, ec);
  if (ec)
    return make_error(sec::cannot_connect_to_node);
  boost::asio::connect(fd, i, ec);
  if (ec)
    return make_error(sec::cannot_connect_to_node);
  return asio_tcp_socket{std::move(fd)};
}

error ip_bind(asio_tcp_socket_acceptor& fd, uint16_t port,
              const char* addr, bool reuse_addr) {
  CAF_LOG_TRACE(CAF_ARG(port) << CAF_ARG(reuse_addr));
  using boost::asio::ip::tcp;
  auto bind_and_listen = [&](tcp::endpoint& ep) -> error {
    CAF_LOG_DEBUG("created IP endpoint:" << CAF_ARG(ep.address().to_string())
                  << CAF_ARG(ep.port()));
    boost::system::error_code ec;
    fd.open(ep.protocol());
    if (reuse_addr) {
      fd.set_option(tcp::acceptor::reuse_address(reuse_addr), ec);
      if (ec)
        return sec::cannot_open_port;
    }
    fd.bind(ep, ec);
    if (ec)
      return sec::cannot_open_port;
    fd.listen(asio_tcp_socket_acceptor::max_connections, ec);
    if (ec)
      return sec::cannot_open_port;
    return none;
  };
  if (addr) {
    CAF_LOG_DEBUG(CAF_ARG(addr));
    tcp::endpoint ep(boost::asio::ip::address::from_string(addr), port);
    CAF_LOG_DEBUG("got 'em");
    return bind_and_listen(ep);
  } else {
    CAF_LOG_DEBUG("addr = nullptr");
    tcp::endpoint ep(tcp::v6(), port);
    return bind_and_listen(ep);
  }
}

scribe_ptr asio_multiplexer::new_scribe(asio_tcp_socket&& sock) {
  CAF_LOG_TRACE("");
  class impl : public scribe {
  public:
    impl(asio_multiplexer& am, asio_tcp_socket&& s)
        : scribe(network::conn_hdl_from_socket(s)),
          launched_(false),
          stream_(am) {
      stream_.init(std::move(s));
    }
    void configure_read(receive_policy::config config) override {
      CAF_LOG_TRACE("");
      stream_.configure_read(config);
      if (!launched_) {
        launch();
      }
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
      return stream_.socket_handle().remote_endpoint().address().to_string();
    }
    uint16_t port() const override {
      return stream_.socket_handle().remote_endpoint().port();
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
    asio_stream<asio_tcp_socket> stream_;
  };
  return make_counted<impl>(*this, std::move(sock));
}

scribe_ptr asio_multiplexer::new_scribe(native_socket fd) {
  CAF_LOG_TRACE(CAF_ARG(fd));
  boost::system::error_code ec;
  asio_tcp_socket sock{service()};
  sock.assign(boost::asio::ip::tcp::v6(), fd, ec);
  if (ec)
    sock.assign(boost::asio::ip::tcp::v4(), fd, ec);
  if (ec)
    CAF_RAISE_ERROR(ec.message());
  return new_scribe(std::move(sock));
}

expected<scribe_ptr> asio_multiplexer::new_tcp_scribe(const std::string& host,
                                                      uint16_t port) {
  auto sck = new_tcp_connection(service(), host, port);
  if (!sck)
    return std::move(sck.error());
  return new_scribe(std::move(*sck));
}

doorman_ptr asio_multiplexer::new_doorman(asio_tcp_socket_acceptor&& sock) {
  CAF_LOG_TRACE(CAF_ARG(sock.native_handle()));
  CAF_ASSERT(sock.native_handle() != network::invalid_native_socket);
  class impl : public doorman {
  public:
    impl(asio_tcp_socket_acceptor&& s, network::asio_multiplexer& am)
        : doorman(network::accept_hdl_from_socket(s)),
          acceptor_(am, s.get_io_service()) {
      acceptor_.init(std::move(s));
    }
    bool new_connection() override {
      CAF_LOG_TRACE("");
      if (detached())
        // we are already disconnected from the broker while the multiplexer
        // did not yet remove the socket, this can happen if an I/O event causes
        // the broker to call close_all() while the pollset contained
        // further activities for the broker
        return false;
      auto& am = acceptor_.backend();
      auto sptr = am.new_scribe(std::move(acceptor_.accepted_socket()));
      auto shdl = sptr->hdl();
      parent()->add_scribe(std::move(sptr));
      return doorman::new_connection(&am, shdl);
    }
    void stop_reading() override {
      CAF_LOG_TRACE("");
      acceptor_.stop();
      detach(&acceptor_.backend(), false);
    }
    void launch() override {
      CAF_LOG_TRACE("");
      acceptor_.start(this);
    }
    std::string addr() const override {
      return
        acceptor_.socket_handle().local_endpoint().address().to_string();
    }
    uint16_t port() const override {
      return acceptor_.socket_handle().local_endpoint().port();
    }
    void add_to_loop() override {
      acceptor_.activate(this);
    }
    void remove_from_loop() override {
      acceptor_.passivate();
    }
  private:
    network::asio_acceptor<asio_tcp_socket_acceptor> acceptor_;
  };
  return make_counted<impl>(std::move(sock), *this);
}

doorman_ptr asio_multiplexer::new_doorman(native_socket fd) {
  CAF_LOG_TRACE(CAF_ARG(fd));
  asio_tcp_socket_acceptor sock{service()};
  boost::system::error_code ec;
  sock.assign(boost::asio::ip::tcp::v6(), fd, ec);
  if (ec)
    sock.assign(boost::asio::ip::tcp::v4(), fd, ec);
  if (ec)
    CAF_RAISE_ERROR(ec.message());
  return new_doorman(std::move(sock));
}

expected<doorman_ptr>
asio_multiplexer::new_tcp_doorman(uint16_t port, const char* in, bool rflag) {
  CAF_LOG_TRACE(CAF_ARG(port) << ", addr = " << (in ? in : "nullptr"));
  asio_tcp_socket_acceptor fd{service()};
  auto err = ip_bind(fd, port, in, rflag);
  if (err)
    return err;
  return new_doorman(std::move(fd));
}

void asio_multiplexer::exec_later(resumable* rptr) {
  auto mt = system().config().scheduler_max_throughput;
  switch (rptr->subtype()) {
    case resumable::io_actor:
    case resumable::function_object: {
      intrusive_ptr<resumable> ptr{rptr, false};
      service().post([=]() mutable {
        switch (ptr->resume(this, mt)) {
          case resumable::resume_later:
            exec_later(ptr.release());
            break;
          case resumable::done:
          case resumable::awaiting_message:
            break;
          default:
            ; // ignored
        }
      });
      break;
    }
    default:
     system().scheduler().enqueue(rptr);
  }
}

asio_multiplexer::asio_multiplexer(actor_system* sys) : multiplexer(sys) {
  // nop
}

asio_multiplexer::~asio_multiplexer() {
  //nop
}

multiplexer::supervisor_ptr asio_multiplexer::make_supervisor() {
  return std::unique_ptr<asio_supervisor>(new asio_supervisor(service()));
}

void asio_multiplexer::run() {
  CAF_LOG_TRACE("asio-based multiplexer");
  boost::system::error_code ec;
  service().run(ec);
  if (ec)
    CAF_RAISE_ERROR(ec.message());
}

boost::asio::io_service* asio_multiplexer::pimpl() {
  return &service_;
}

} // namesapce network
} // namespace io
} // namespace caf
