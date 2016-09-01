/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#ifndef CAF_IO_NETWORK_ASIO_MULTIPLEXER_HPP
#define CAF_IO_NETWORK_ASIO_MULTIPLEXER_HPP

#include "caf/config.hpp"

CAF_PUSH_WARNINGS
#include "boost/asio.hpp"
CAF_POP_WARNINGS

#include "caf/logger.hpp"

#include "caf/io/receive_policy.hpp"

#include "caf/io/network/multiplexer.hpp"
#include "caf/io/network/native_socket.hpp"
#include "caf/io/network/stream_manager.hpp"
#include "caf/io/network/acceptor_manager.hpp"

namespace caf {
namespace io {
namespace network {

/// Low-level error code.
using error_code = boost::system::error_code;

/// Low-level backend for IO multiplexing.
using io_service = boost::asio::io_service;

/// Low-level socket type used as default.
using asio_tcp_socket = boost::asio::ip::tcp::socket;

/// Low-level socket type used as default.
using asio_tcp_socket_acceptor = boost::asio::ip::tcp::acceptor;

/// A wrapper for the boost::asio multiplexer
class asio_multiplexer : public multiplexer {
public:
  friend class io::middleman;
  friend class supervisor;

  expected<connection_handle>
  new_tcp_scribe(const std::string&, uint16_t) override;

  expected<void>
  assign_tcp_scribe(abstract_broker*, connection_handle hdl) override;

  template <class Socket>
  connection_handle add_tcp_scribe(abstract_broker*, Socket&& sock);

  connection_handle add_tcp_scribe(abstract_broker*, native_socket fd) override;

  expected<connection_handle>
  add_tcp_scribe(abstract_broker*, const std::string&, uint16_t) override;

  expected<std::pair<accept_handle, uint16_t>>
  new_tcp_doorman(uint16_t p, const char* in, bool rflag) override;

  expected<void>
  assign_tcp_doorman(abstract_broker*, accept_handle hdl) override;

  accept_handle add_tcp_doorman(abstract_broker*,
                                asio_tcp_socket_acceptor&& sock);

  accept_handle add_tcp_doorman(abstract_broker*, native_socket fd) override;

  expected<std::pair<accept_handle, uint16_t>>
  add_tcp_doorman(abstract_broker*, uint16_t, const char*, bool) override;

  void exec_later(resumable* ptr) override;

  asio_multiplexer(actor_system* sys);

  ~asio_multiplexer();

  supervisor_ptr make_supervisor() override;

  void run() override;

  boost::asio::io_service* pimpl() override;

  inline boost::asio::io_service& service() {
    return service_;
  }

private:
  io_service service_;
  std::mutex mtx_sockets_;
  std::mutex mtx_acceptors_;
  std::map<int64_t, asio_tcp_socket> unassigned_sockets_;
  std::map<int64_t, asio_tcp_socket_acceptor> unassigned_acceptors_;
};

template <class T>
connection_handle conn_hdl_from_socket(T& sock) {
  return connection_handle::from_int(
    int64_from_native_socket(sock.native_handle()));
}

template <class T>
accept_handle accept_hdl_from_socket(T& sock) {
  return accept_handle::from_int(
    int64_from_native_socket(sock.native_handle()));
}

/// @relates manager
using manager_ptr = intrusive_ptr<manager>;

/// A stream capable of both reading and writing. The stream's input
/// data is forwarded to its {@link stream_manager manager}.
template <class Socket>
class asio_stream {
public:
  /// A smart pointer to a stream manager.
  using manager_ptr = intrusive_ptr<stream_manager>;

  /// A buffer class providing a compatible interface to `std::vector`.
  using buffer_type = std::vector<char>;

  asio_stream(asio_multiplexer& ref)
      : reading_(false),
        writing_(false),
        ack_writes_(false),
        fd_(ref.service()),
        backend_(ref),
        rd_buf_ready_(false),
        async_read_pending_(false) {
    configure_read(receive_policy::at_most(1024));
  }

  /// Returns the IO socket.
  Socket& socket_handle() {
    return fd_;
  }

  /// Returns the IO socket.
  const Socket& socket_handle() const {
    return fd_;
  }

  /// Initializes this stream, setting the socket handle to `fd`.
  void init(Socket fd) {
    fd_ = std::move(fd);
  }

  /// Starts reading data from the socket, forwarding incoming data to `mgr`.
  void start(stream_manager* mgr) {
    CAF_ASSERT(mgr != nullptr);
    activate(mgr);
  }

  /// Configures how much data will be provided for the next `consume` callback.
  /// @warning Must not be called outside the IO multiplexers event loop
  ///          once the stream has been started.
  void configure_read(receive_policy::config config) {
    rd_flag_ = config.first;
    rd_size_ = config.second;
  }

  void ack_writes(bool enable) {
    CAF_LOG_TRACE(CAF_ARG(enable));
    ack_writes_ = enable;
  }

  /// Copies data to the write buffer.
  /// @note Not thread safe.
  void write(const void* buf, size_t num_bytes) {
    CAF_LOG_TRACE(CAF_ARG(num_bytes));
    auto first = reinterpret_cast<const char*>(buf);
    auto last = first + num_bytes;
    wr_offline_buf_.insert(wr_offline_buf_.end(), first, last);
  }

  /// Returns the write buffer of this stream.
  /// @warning Must not be modified outside the IO multiplexers event loop
  ///          once the stream has been started.
  buffer_type& wr_buf() {
    return wr_offline_buf_;
  }

  buffer_type& rd_buf() {
    return rd_buf_;
  }

  /// Sends the content of the write buffer, calling the `io_failure`
  /// member function of `mgr` in case of an error.
  /// @warning Must not be called outside the IO multiplexers event loop
  ///          once the stream has been started.
  void flush(const manager_ptr& mgr) {
    CAF_ASSERT(mgr != nullptr);
    if (!wr_offline_buf_.empty() && !writing_) {
      writing_ = true;
      write_loop(mgr);
    }
  }

  /// Closes the network connection, thus stopping this stream.
  void stop() {
    CAF_LOG_TRACE("");
    fd_.close();
  }

  void stop_reading() {
    CAF_LOG_TRACE("");
    error_code ec; // ignored
    fd_.shutdown(boost::asio::ip::tcp::socket::shutdown_receive, ec);
  }

  asio_multiplexer& backend() {
    return backend_;
  }

  /// Activates the stream.
  void activate(stream_manager* mgr) {
    reading_ = true;
    read_loop(mgr);
  }

  /// Stops activity of the stream.
  void passivate() {
    reading_ = false;
  }

private:
  bool read_one(stream_manager* ptr, size_t num_bytes) {
    if (!reading_) {
      // broker was passivated while async read was on its way
      rd_buf_ready_ = true;
      // make sure buf size matches read_bytes in case of async_read
      if (rd_buf_.size() != num_bytes)
        rd_buf_.resize(num_bytes);
      return false;
    }
    if (ptr->consume(&backend(), rd_buf_.data(), num_bytes))
      return reading_;
    return false;
  }

  void read_loop(manager_ptr mgr) {
    if (async_read_pending_)
      return;
    if (rd_buf_ready_) {
      rd_buf_ready_ = false;
      if (read_one(mgr.get(), rd_buf_.size()))
        read_loop(std::move(mgr));
      return;
    }
    auto cb = [=](const error_code& ec, size_t read_bytes) mutable {
      async_read_pending_ = false;
      CAF_LOG_TRACE("");
      if (!ec) {
        // bail out early in case broker passivated stream in the meantime
        if (read_one(mgr.get(), read_bytes))
          read_loop(std::move(mgr));
      } else {
        mgr->io_failure(&backend(), operation::read);
      }
    };
    switch (rd_flag_) {
      case receive_policy_flag::exactly:
        if (rd_buf_.size() < rd_size_)
          rd_buf_.resize(rd_size_);
        async_read_pending_ = true;
        boost::asio::async_read(fd_, boost::asio::buffer(rd_buf_, rd_size_),
                                cb);
        break;
      case receive_policy_flag::at_most:
        if (rd_buf_.size() < rd_size_)
          rd_buf_.resize(rd_size_);
        async_read_pending_ = true;
        fd_.async_read_some(boost::asio::buffer(rd_buf_, rd_size_), cb);
        break;
      case receive_policy_flag::at_least: {
        // read up to 10% more, but at least allow 100 bytes more
        auto min_size = rd_size_ + std::max<size_t>(100, rd_size_ / 10);
        if (rd_buf_.size() < min_size) {
          rd_buf_.resize(min_size);
        }
        collect_data(mgr, 0);
        break;
      }
    }
  }

  void write_loop(const manager_ptr& mgr) {
    if (wr_offline_buf_.empty()) {
      writing_ = false;
      return;
    }
    wr_buf_.clear();
    wr_buf_.swap(wr_offline_buf_);
    boost::asio::async_write(
      fd_, boost::asio::buffer(wr_buf_),
      [=](const error_code& ec, size_t nb) {
        CAF_LOG_TRACE("");
        if (ec) {
          CAF_LOG_DEBUG(CAF_ARG(ec.message()));
          mgr->io_failure(&backend(), operation::read);
          writing_ = false;
          return;
        }
        CAF_LOG_DEBUG(CAF_ARG(nb));
        if (ack_writes_)
          mgr->data_transferred(&backend(), nb, wr_offline_buf_.size());
        write_loop(mgr);
      });
  }

  void collect_data(manager_ptr mgr, size_t collected_bytes) {
    async_read_pending_ = true;
    fd_.async_read_some(boost::asio::buffer(rd_buf_.data() + collected_bytes,
                                            rd_buf_.size() - collected_bytes),
                        [=](const error_code& ec, size_t nb) mutable {
      async_read_pending_ = false;
      CAF_LOG_TRACE(CAF_ARG(nb));
      if (!ec) {
        auto sum = collected_bytes + nb;
        if (sum >= rd_size_) {
          if (read_one(mgr.get(), sum))
            read_loop(std::move(mgr));
        } else {
          collect_data(std::move(mgr), sum);
        }
      } else {
        mgr->io_failure(&backend(), operation::write);
      }
    });
  }

  /// Set if read loop was started by user and unset if passivate is called.
  bool reading_;

  /// Set on flush, also indicates that an async_write is pending.
  bool writing_;

  /// Stores whether user requested ACK messages for async writes.
  bool ack_writes_;

  /// TCP socket for this connection.
  Socket fd_;

  /// Configures how chunk sizes are calculated.
  receive_policy_flag rd_flag_;

  /// Minimum, maximum, or exact size of a chunk, depending on `rd_flag_`.
  size_t rd_size_;

  /// Input buffer.
  buffer_type rd_buf_;

  /// Output buffer for ASIO.
  buffer_type wr_buf_;

  /// Swapped with `wr_buf_` before next write. Users write into this buffer
  /// as long as `wr_buf_` is processed by ASIO.
  buffer_type wr_offline_buf_;

  /// Reference to our I/O backend.
  asio_multiplexer& backend_;

  /// Signalizes that a scribe was passivated while an async read was pending.
  bool rd_buf_ready_;

  /// Makes sure no more than one async_read is pending at any given time
  bool async_read_pending_;
};

/// An acceptor is responsible for accepting incoming connections.
template <class SocketAcceptor>
class asio_acceptor {
  using protocol_type = typename SocketAcceptor::protocol_type;
  using socket_type = boost::asio::basic_stream_socket<protocol_type>;

public:
  /// A manager providing the `accept` member function.
  using manager_type = acceptor_manager;

  /// A smart pointer to an acceptor manager.
  using manager_ptr = intrusive_ptr<manager_type>;

  asio_acceptor(asio_multiplexer& am, io_service& io)
      : accepting_(false),
        backend_(am),
        accept_fd_(io),
        fd_valid_(false),
        fd_(io),
        async_accept_pending_(false) {
    // nop
  }

  /// Returns the `multiplexer` this acceptor belongs to.
  asio_multiplexer& backend() { return backend_; }

  /// Returns the IO socket.
  SocketAcceptor& socket_handle() {
    return accept_fd_;
  }

  /// Returns the IO socket.
  const SocketAcceptor& socket_handle() const {
    return accept_fd_;
  }

  /// Returns the accepted socket. This member function should
  ///        be called only from the `new_connection` callback.
  inline socket_type& accepted_socket() {
    return fd_;
  }

  /// Initializes this acceptor, setting the socket handle to `fd`.
  void init(SocketAcceptor fd) {
    accept_fd_ = std::move(fd);
  }

  /// Starts this acceptor, forwarding all incoming connections to
  /// `manager`. The intrusive pointer will be released after the
  /// acceptor has been closed or an IO error occured.
  void start(manager_type* mgr) {
    activate(mgr);
  }

  /// Starts the accept loop.
  void activate(manager_type* mgr) {
    accepting_ = true;
    accept_loop(mgr);
  }

  /// Starts the accept loop.
  void passivate() {
    accepting_ = false;
  }

  /// Closes the network connection, thus stopping this acceptor.
  void stop() {
    accept_fd_.close();
  }

private:
  bool accept_one(manager_type* mgr) {
    auto res = mgr->new_connection(); // moves fd_
    // reset fd_ for next accept operation
    fd_ = socket_type{accept_fd_.get_io_service()};
    return res && accepting_;
  }

  void accept_loop(manager_ptr mgr) {
    if (async_accept_pending_)
      return;
    // accept "cached" connection first
    if (fd_valid_) {
      fd_valid_ = false;
      if (accept_one(mgr.get()))
        accept_loop(std::move(mgr));
      return;
    }
    async_accept_pending_ = true;
    accept_fd_.async_accept(fd_, [=](const error_code& ec) mutable {
      CAF_LOG_TRACE("");
      async_accept_pending_ = false;
      if (!ec) {
        // if broker has passivated this in the meantime, cache fd_ for later
        if (!accepting_) {
          fd_valid_ = true;
          return;
        }
        if (accept_one(mgr.get()))
          accept_loop(std::move(mgr));
      } else {
        mgr->io_failure(&backend(), operation::read);
      }
    });
  }

  bool accepting_;
  asio_multiplexer& backend_;
  SocketAcceptor accept_fd_;
  bool fd_valid_;
  socket_type fd_;
  /// Makes sure no more than one async_accept is pending at any given time
  bool async_accept_pending_;
};

} // namesapce network
} // namespace io
} // namespace caf

#endif // CAF_IO_NETWORK_ASIO_MULTIPLEXER_HPP
