/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#include "caf/exception.hpp"

#include "caf/io/broker.hpp"
#include "caf/io/middleman.hpp"

#include "caf/io/network/asio_multiplexer.hpp"

namespace caf {
namespace io {
namespace network {

namespace {

/**
 * A wrapper for the supervisor backend provided by boost::asio.
 */
struct asio_supervisor : public multiplexer::supervisor {
  asio_supervisor(io_backend& iob) : work(iob) {
    // nop
  }

 private:
  boost::asio::io_service::work work;
};

} // namespace anonymous

default_socket new_tcp_connection(io_backend& backend, const std::string& host,
                                  uint16_t port) {
  default_socket fd{backend};
  using boost::asio::ip::tcp;
  tcp::resolver r(fd.get_io_service());
  tcp::resolver::query q(host, std::to_string(port));
  boost::system::error_code ec;
  auto i = r.resolve(q, ec);
  if (ec) {
    throw network_error("could not resolve host: " + host);
  }
  boost::asio::connect(fd, i, ec);
  if (ec) {
    throw network_error("could not connect to host: " + host +
                        ":" + std::to_string(port));
  }
  return fd;
}

void ip_bind(default_socket_acceptor& fd, uint16_t port,
             const char* addr = nullptr, bool reuse_addr = true) {
  CAF_LOGF_TRACE(CAF_ARG(port));
  using boost::asio::ip::tcp;
  try {
    auto bind_and_listen = [&](tcp::endpoint& ep) {
      fd.open(ep.protocol());
      fd.set_option(tcp::acceptor::reuse_address(reuse_addr));
      fd.bind(ep);
      fd.listen();
    };
    if (addr) {
      tcp::endpoint ep(boost::asio::ip::address::from_string(addr), port);
      bind_and_listen(ep);
      CAF_LOGF_DEBUG("created IPv6 endpoint: " << ep.address() << ":"
                                               << fd.local_endpoint().port());
    } else {
      tcp::endpoint ep(tcp::v6(), port);
      bind_and_listen(ep);
      CAF_LOGF_DEBUG("created IPv6 endpoint: " << ep.address() << ":"
                                               << fd.local_endpoint().port());
    }
  } catch (boost::system::system_error& se) {
    if (se.code() == boost::system::errc::address_in_use) {
      throw bind_failure(se.code().message());
    }
    throw network_error(se.code().message());
  }
}

connection_handle asio_multiplexer::new_tcp_scribe(const std::string& host,
                                                   uint16_t port) {
  default_socket fd{new_tcp_connection(backend(), host, port)};
  auto id = int64_from_native_socket(fd.native_handle());
  std::lock_guard<std::mutex> lock(m_mtx_sockets);
  m_unassigned_sockets.insert(std::make_pair(id, std::move(fd)));
  return connection_handle::from_int(id);
}

void asio_multiplexer::assign_tcp_scribe(broker* self, connection_handle hdl) {
  std::lock_guard<std::mutex> lock(m_mtx_sockets);
  auto itr = m_unassigned_sockets.find(hdl.id());
  if (itr != m_unassigned_sockets.end()) {
    add_tcp_scribe(self, std::move(itr->second));
    m_unassigned_sockets.erase(itr);
  }
}

template <class Socket>
connection_handle asio_multiplexer::add_tcp_scribe(broker* self,
                                                   Socket&& sock) {
  CAF_LOG_TRACE("");
  class impl : public broker::scribe {
   public:
    impl(broker* ptr, Socket&& s)
        : scribe(ptr, network::conn_hdl_from_socket(s)),
          m_launched(false),
          m_stream(s.get_io_service()) {
      m_stream.init(std::move(s));
    }
    void configure_read(receive_policy::config config) override {
      CAF_LOG_TRACE("");
      m_stream.configure_read(config);
      if (!m_launched) {
        launch();
      }
    }
    broker::buffer_type& wr_buf() override { return m_stream.wr_buf(); }
    broker::buffer_type& rd_buf() override { return m_stream.rd_buf(); }
    void stop_reading() override {
      CAF_LOG_TRACE("");
      m_stream.stop_reading();
      disconnect(false);
    }
    void flush() override {
      CAF_LOG_TRACE("");
      m_stream.flush(this);
    }
    void launch() {
      CAF_LOG_TRACE("");
      CAF_ASSERT(!m_launched);
      m_launched = true;
      m_stream.start(this);
    }

   private:
    bool m_launched;
    stream<Socket> m_stream;
  };
  broker::scribe_pointer ptr = make_counted<impl>(self, std::move(sock));
  self->add_scribe(ptr);
  return ptr->hdl();
}

connection_handle asio_multiplexer::add_tcp_scribe(broker* self,
                                                   native_socket fd) {
  CAF_LOG_TRACE(CAF_ARG(self) << ", " << CAF_ARG(fd));
  boost::system::error_code ec;
  default_socket sock{backend()};
  sock.assign(boost::asio::ip::tcp::v6(), fd, ec);
  if (ec) {
    sock.assign(boost::asio::ip::tcp::v4(), fd, ec);
  }
  if (ec) {
    throw network_error(ec.message());
  }
  return add_tcp_scribe(self, std::move(sock));
}

connection_handle asio_multiplexer::add_tcp_scribe(broker* self,
                                                   const std::string& host,
                                                   uint16_t port) {
  CAF_LOG_TRACE(CAF_ARG(self) << ", " << CAF_ARG(host) << ":" << CAF_ARG(port));
  return add_tcp_scribe(self, new_tcp_connection(backend(), host, port));
}

std::pair<accept_handle, uint16_t>
asio_multiplexer::new_tcp_doorman(uint16_t port, const char* in, bool rflag) {
  CAF_LOG_TRACE(CAF_ARG(port) << ", addr = " << (in ? in : "nullptr"));
  default_socket_acceptor fd{backend()};
  ip_bind(fd, port, in, rflag);
  auto id = int64_from_native_socket(fd.native_handle());
  auto assigned_port = fd.local_endpoint().port();
  std::lock_guard<std::mutex> lock(m_mtx_acceptors);
  m_unassigned_acceptors.insert(std::make_pair(id, std::move(fd)));
  return {accept_handle::from_int(id), assigned_port};
}

void asio_multiplexer::assign_tcp_doorman(broker* self, accept_handle hdl) {
  CAF_LOG_TRACE("");
  std::lock_guard<std::mutex> lock(m_mtx_acceptors);
  auto itr = m_unassigned_acceptors.find(hdl.id());
  if (itr != m_unassigned_acceptors.end()) {
    add_tcp_doorman(self, std::move(itr->second));
    m_unassigned_acceptors.erase(itr);
  }
}

accept_handle
asio_multiplexer::add_tcp_doorman(broker* self,
                                  default_socket_acceptor&& sock) {
  CAF_LOG_TRACE("sock.fd = " << sock.native_handle());
  CAF_ASSERT(sock.native_handle() != network::invalid_native_socket);
  class impl : public broker::doorman {
   public:
    impl(broker* ptr, default_socket_acceptor&& s,
         network::asio_multiplexer& am)
        : doorman(ptr, network::accept_hdl_from_socket(s)),
          m_acceptor(am, s.get_io_service()) {
      m_acceptor.init(std::move(s));
    }
    void new_connection() override {
      CAF_LOG_TRACE("");
      auto& am = m_acceptor.backend();
      accept_msg().handle
        = am.add_tcp_scribe(parent(), std::move(m_acceptor.accepted_socket()));
      parent()->invoke_message(invalid_actor_addr, invalid_message_id,
                               m_accept_msg);
    }
    void stop_reading() override {
      CAF_LOG_TRACE("");
      m_acceptor.stop();
      disconnect(false);
    }
    void launch() override {
      CAF_LOG_TRACE("");
      m_acceptor.start(this);
    }

   private:
    network::acceptor<default_socket_acceptor> m_acceptor;
  };
  broker::doorman_pointer ptr
    = make_counted<impl>(self, std::move(sock), *this);
  self->add_doorman(ptr);
  return ptr->hdl();
}

accept_handle asio_multiplexer::add_tcp_doorman(broker* self,
                                                native_socket fd) {
  default_socket_acceptor sock{backend()};
  boost::system::error_code ec;
  sock.assign(boost::asio::ip::tcp::v6(), fd, ec);
  if (ec) {
    sock.assign(boost::asio::ip::tcp::v4(), fd, ec);
  }
  if (ec) {
    throw network_error(ec.message());
  }
  return add_tcp_doorman(self, std::move(sock));
}

std::pair<accept_handle, uint16_t>
asio_multiplexer::add_tcp_doorman(broker* self, uint16_t port, const char* in,
                                  bool rflag) {
  default_socket_acceptor fd{backend()};
  ip_bind(fd, port, in, rflag);
  auto p = fd.local_endpoint().port();
  return {add_tcp_doorman(self, std::move(fd)), p};
}

void asio_multiplexer::dispatch_runnable(runnable_ptr ptr) {
  backend().post([=]() {
    ptr->run();
  });
}

asio_multiplexer::asio_multiplexer() {
  // nop
}

asio_multiplexer::~asio_multiplexer() {
  //nop
}

multiplexer::supervisor_ptr asio_multiplexer::make_supervisor() {
  return std::unique_ptr<asio_supervisor>(new asio_supervisor(backend()));
}

void asio_multiplexer::run() {
  CAF_LOG_TRACE("asio-based multiplexer");
  boost::system::error_code ec;
  backend().run(ec);
  if (ec) {
    throw std::runtime_error(ec.message());
  }
}

asio_multiplexer& get_multiplexer_singleton() {
  return static_cast<asio_multiplexer&>(middleman::instance()->backend());
}

} // namesapce network
} // namespace io
} // namespace caf
