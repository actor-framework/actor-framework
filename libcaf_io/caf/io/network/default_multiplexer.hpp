/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#ifndef CAF_IO_NETWORK_DEFAULT_MULTIPLEXER_HPP
#define CAF_IO_NETWORK_DEFAULT_MULTIPLEXER_HPP

#include <thread>

#include <vector>
#include <string>
#include <cstdint>

#include "caf/config.hpp"
#include "caf/extend.hpp"
#include "caf/exception.hpp"
#include "caf/ref_counted.hpp"

#include "caf/io/fwd.hpp"
#include "caf/io/accept_handle.hpp"
#include "caf/io/receive_policy.hpp"
#include "caf/io/connection_handle.hpp"
#include "caf/io/network/operation.hpp"
#include "caf/io/network/multiplexer.hpp"
#include "caf/io/network/stream_manager.hpp"
#include "caf/io/network/acceptor_manager.hpp"

#include "caf/io/network/native_socket.hpp"

#include "caf/detail/logging.hpp"

#ifdef CAF_WINDOWS
# define WIN32_LEAN_AND_MEAN
# define NOMINMAX
# ifdef CAF_MINGW
#   undef _WIN32_WINNT
#   undef WINVER
#   define _WIN32_WINNT WindowsVista
#   define WINVER WindowsVista
#   include <w32api.h>
# endif
# include <windows.h>
# include <winsock2.h>
# include <ws2tcpip.h>
# include <ws2ipdef.h>
#else
# include <unistd.h>
# include <errno.h>
# include <sys/socket.h>
#endif

// poll xs epoll backend
#if ! defined(CAF_LINUX) || defined(CAF_POLL_IMPL) // poll() multiplexer
# define CAF_POLL_MULTIPLEXER
# ifndef CAF_WINDOWS
#   include <poll.h>
# endif
# ifndef POLLRDHUP
#   define POLLRDHUP POLLHUP
# endif
# ifndef POLLPRI
#   define POLLPRI POLLIN
# endif
#else
# define CAF_EPOLL_MULTIPLEXER
# include <sys/epoll.h>
#endif

namespace caf {
namespace io {
namespace network {

// annoying platform-dependent bootstrapping
#ifdef CAF_WINDOWS
  using setsockopt_ptr = const char*;
  using socket_send_ptr = const char*;
  using socket_recv_ptr = char*;
  using socklen_t = int;
  using ssize_t = std::make_signed<size_t>::type;
  inline int last_socket_error() { return WSAGetLastError(); }
  inline bool would_block_or_temporarily_unavailable(int errcode) {
    return errcode == WSAEWOULDBLOCK || errcode == WSATRY_AGAIN;
  }
  constexpr int ec_out_of_memory = WSAENOBUFS;
  constexpr int ec_interrupted_syscall = WSAEINTR;
#else
  using setsockopt_ptr = const void*;
  using socket_send_ptr = const void*;
  using socket_recv_ptr = void*;
  inline void closesocket(int fd) { close(fd); }
  inline int last_socket_error() { return errno; }
  inline bool would_block_or_temporarily_unavailable(int errcode) {
    return errcode == EAGAIN || errcode == EWOULDBLOCK;
  }
  constexpr int ec_out_of_memory = ENOMEM;
  constexpr int ec_interrupted_syscall = EINTR;
#endif

// poll xs epoll backend
#if ! defined(CAF_LINUX) || defined(CAF_POLL_IMPL) // poll() multiplexer
# ifdef CAF_WINDOWS
    // From the MSDN: If the POLLPRI flag is set on a socket for the Microsoft
    //                Winsock provider, the WSAPoll function will fail.
    constexpr short input_mask  = POLLIN;
# else
    constexpr short input_mask  = POLLIN | POLLPRI;
# endif
  constexpr short error_mask  = POLLRDHUP | POLLERR | POLLHUP | POLLNVAL;
  constexpr short output_mask = POLLOUT;
  class event_handler;
  using multiplexer_data = pollfd;
  using multiplexer_poll_shadow_data = std::vector<event_handler*>;
#else
# define CAF_EPOLL_MULTIPLEXER
  constexpr int input_mask  = EPOLLIN;
  constexpr int error_mask  = EPOLLRDHUP | EPOLLERR | EPOLLHUP;
  constexpr int output_mask = EPOLLOUT;
  using multiplexer_data = epoll_event;
  using multiplexer_poll_shadow_data = native_socket;
#endif

/// Platform-specific native acceptor socket type.
using native_socket_acceptor = native_socket;

/// Returns the last socket error as human-readable string.
std::string last_socket_error_as_string();

/// Sets fd to nonblocking if `set_nonblocking == true`
/// or to blocking if `set_nonblocking == false`
/// throws `network_error` on error
void nonblocking(native_socket fd, bool new_value);

/// Creates two connected sockets. The former is the read handle
/// and the latter is the write handle.
std::pair<native_socket, native_socket> create_pipe();

/// Returns true if `fd` is configured as nodelay socket.
/// @throws network_error
void tcp_nodelay(native_socket fd, bool new_value);

/// Throws `network_error` if `result` is invalid.
void handle_write_result(ssize_t result);

/// Throws `network_error` if `result` is invalid.
void handle_read_result(ssize_t result);

/// Reads up to `len` bytes from `fd,` writing the received data
/// to `buf`. Returns `true` as long as `fd` is readable and `false`
/// if the socket has been closed or an IO error occured. The number
/// of read bytes is stored in `result` (can be 0).
bool read_some(size_t& result, native_socket fd, void* buf, size_t len);

/// Writes up to `len` bytes from `buf` to `fd`.
/// Returns `true` as long as `fd` is readable and `false`
/// if the socket has been closed or an IO error occured. The number
/// of written bytes is stored in `result` (can be 0).
bool write_some(size_t& result, native_socket fd, const void* buf, size_t len);

/// Tries to accept a new connection from `fd`. On success,
/// the new connection is stored in `result`. Returns true
/// as long as
bool try_accept(native_socket& result, native_socket fd);

class default_multiplexer;

/// A socket IO event handler.
class event_handler {
public:
  event_handler(default_multiplexer& dm);

  virtual ~event_handler();

  /// Returns true once the requested operation is done, i.e.,
  /// to signalize the multiplexer to remove this handler.
  /// The handler remains in the event loop as long as it returns false.
  virtual void handle_event(operation op) = 0;

  /// Callback to signalize that this handler has been removed
  /// from the event loop for operations of type `op`.
  virtual void removed_from_loop(operation op) = 0;

  /// Returns the native socket handle for this handler.
  virtual native_socket fd() const = 0;

  /// Returns the `multiplexer` this acceptor belongs to.
  inline default_multiplexer& backend() {
    return backend_;
  }

  /// Returns the bit field storing the subscribed events.
  inline int eventbf() const {
    return eventbf_;
  }

  /// Sets the bit field storing the subscribed events.
  inline void eventbf(int value) {
    eventbf_ = value;
  }

protected:
  default_multiplexer& backend_;
  int eventbf_;
};

/// Low-level socket type used as default.
class default_socket {
public:
  using socket_type = default_socket;

  default_socket(default_multiplexer& parent,
                 native_socket sock = invalid_native_socket);

  default_socket(default_socket&& other);

  default_socket& operator=(default_socket&& other);

  ~default_socket();

  void close_read();

  inline native_socket fd() const {
    return fd_;
  }

  inline native_socket native_handle() const {
    return fd_;
  }

  inline default_multiplexer& backend() {
    return parent_;
  }

private:
  default_multiplexer& parent_;
  native_socket fd_;
};

/// Low-level socket type used as default.
using default_socket_acceptor = default_socket;

class default_multiplexer : public multiplexer {
public:
  friend class io::middleman; // disambiguate reference
  friend class supervisor;

  struct event {
    native_socket fd;
    int mask;
    event_handler* ptr;
  };

  struct event_less {
    inline bool operator()(native_socket lhs, const event& rhs) const {
      return lhs < rhs.fd;
    }
    inline bool operator()(const event& lhs, native_socket rhs) const {
      return lhs.fd < rhs;
    }
    inline bool operator()(const event& lhs, const event& rhs) const {
      return lhs.fd < rhs.fd;
    }
  };

  connection_handle new_tcp_scribe(const std::string&, uint16_t) override;

  void assign_tcp_scribe(abstract_broker* ptr, connection_handle hdl) override;

  connection_handle add_tcp_scribe(abstract_broker*, default_socket_acceptor&& sock);

  connection_handle add_tcp_scribe(abstract_broker*, native_socket fd) override;

  connection_handle add_tcp_scribe(abstract_broker*, const std::string& h,
                                    uint16_t port) override;

  std::pair<accept_handle, uint16_t>
  new_tcp_doorman(uint16_t p, const char* in, bool rflag) override;

  void assign_tcp_doorman(abstract_broker* ptr, accept_handle hdl) override;

  accept_handle add_tcp_doorman(abstract_broker*, default_socket_acceptor&& sock);

  accept_handle add_tcp_doorman(abstract_broker*, native_socket fd) override;

  std::pair<accept_handle, uint16_t>
  add_tcp_doorman(abstract_broker*, uint16_t p, const char* in, bool rflag) override;

  void dispatch_runnable(runnable_ptr ptr) override;

  default_multiplexer();

  ~default_multiplexer();

  supervisor_ptr make_supervisor() override;

  void run() override;

  void add(operation op, native_socket fd, event_handler* ptr);

  void del(operation op, native_socket fd, event_handler* ptr);

private:
  // platform-dependent additional initialization code
  void init();

  template <class F>
  void new_event(F fun, operation op, native_socket fd, event_handler* ptr) {
    CAF_ASSERT(fd != invalid_native_socket);
    CAF_ASSERT(ptr != nullptr || fd == pipe_.first);
    // the only valid input where ptr == nullptr is our pipe
    // read handle which is only registered for reading
    auto old_bf = ptr ? ptr->eventbf() : input_mask;
    //auto bf = fun(op, old_bf);
    CAF_LOG_TRACE(CAF_TSARG(op) << ", " << CAF_ARG(fd) << ", " CAF_ARG(ptr)
                  << ", " << CAF_ARG(old_bf));
    auto last = events_.end();
    auto i = std::lower_bound(events_.begin(), last, fd, event_less{});
    if (i != last && i->fd == fd) {
      CAF_ASSERT(ptr == i->ptr);
      // squash events together
      CAF_LOG_DEBUG("squash events: " << i->mask << " -> "
               << fun(op, i->mask));
      auto bf = i->mask;
      i->mask = fun(op, bf);
      if (i->mask == bf) {
        // didn't do a thing
        CAF_LOG_DEBUG("squashing did not change the event");
      } else if (i->mask == old_bf) {
        // just turned into a nop
        CAF_LOG_DEBUG("squashing events resulted in a NOP");
        events_.erase(i);
      }
    } else {
      // insert new element
      auto bf = fun(op, old_bf);
      if (bf == old_bf) {
        CAF_LOG_DEBUG("event has no effect (discarded): "
                 << CAF_ARG(bf) << ", " << CAF_ARG(old_bf));
      } else {
        CAF_LOG_DEBUG("added handler " << ptr << " on fd " << fd << " for "
                      << to_string(op) << " operations");
        events_.insert(i, event{fd, bf, ptr});
      }
    }
  }

  void handle(const event& event);

  bool socket_had_rd_shutdown_event(native_socket fd);

  void handle_socket_event(native_socket fd, int mask, event_handler* ptr);

  void close_pipe();

  void wr_dispatch_request(runnable* ptr);

  runnable* rd_dispatch_request();

  native_socket epollfd_; // unused in poll() implementation
  std::vector<multiplexer_data> pollset_;
  std::vector<event> events_; // always sorted by .fd
  multiplexer_poll_shadow_data shadow_;
  std::pair<native_socket, native_socket> pipe_;
};

default_multiplexer& get_multiplexer_singleton();

template <class T>
connection_handle conn_hdl_from_socket(const T& sock) {
  return connection_handle::from_int(
        int64_from_native_socket(sock.native_handle()));
}

template <class T>
accept_handle accept_hdl_from_socket(const T& sock) {
  return accept_handle::from_int(
        int64_from_native_socket(sock.native_handle()));
}


/// A stream capable of both reading and writing. The stream's input
/// data is forwarded to its {@link stream_manager manager}.
template <class Socket>
class stream : public event_handler {
public:
  /// A smart pointer to a stream manager.
  using manager_ptr = intrusive_ptr<stream_manager>;

  /// A buffer class providing a compatible
  /// interface to `std::vector`.
  using buffer_type = std::vector<char>;

  stream(default_multiplexer& backend_ref)
      : event_handler(backend_ref),
        sock_(backend_ref),
        threshold_(1),
        collected_(0),
        writing_(false),
        written_(0) {
    configure_read(receive_policy::at_most(1024));
  }

  /// Returns the IO socket.
  Socket& socket_handle() {
    return sock_;
  }

  /// Initializes this stream, setting the socket handle to `fd`.
  void init(Socket sockfd) {
    sock_ = std::move(sockfd);
  }

  /// Starts reading data from the socket, forwarding incoming data to `mgr`.
  void start(const manager_ptr& mgr) {
    CAF_ASSERT(mgr != nullptr);
    reader_ = mgr;
    backend().add(operation::read, sock_.fd(), this);
    read_loop();
  }

  void removed_from_loop(operation op) override {
    switch (op) {
      case operation::read:  reader_.reset(); break;
      case operation::write: writer_.reset(); break;
      case operation::propagate_error: break;
    }
  }

  /// Configures how much data will be provided for the next `consume` callback.
  /// @warning Must not be called outside the IO multiplexers event loop
  ///          once the stream has been started.
  void configure_read(receive_policy::config config) {
    rd_flag_ = config.first;
    max_ = config.second;
  }

  /// Copies data to the write buffer.
  /// @warning Not thread safe.
  void write(const void* buf, size_t num_bytes) {
    CAF_LOG_TRACE("num_bytes: " << num_bytes);
    auto first = reinterpret_cast<const char*>(buf);
    auto last  = first + num_bytes;
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
    CAF_LOG_TRACE("offline buf size: " << wr_offline_buf_.size()
             << ", mgr = " << mgr.get()
             << ", writer_ = " << writer_.get());
    if (! wr_offline_buf_.empty() && ! writing_) {
      backend().add(operation::write, sock_.fd(), this);
      writer_ = mgr;
      writing_ = true;
      write_loop();
    }
  }

  void stop_reading() {
    CAF_LOG_TRACE("");
    sock_.close_read();
    backend().del(operation::read, sock_.fd(), this);
  }

  void handle_event(operation op) override {
    CAF_LOG_TRACE("op = " << static_cast<int>(op));
    switch (op) {
      case operation::read: {
        size_t rb; // read bytes
        if (! read_some(rb, sock_.fd(),
                 rd_buf_.data() + collected_,
                 rd_buf_.size() - collected_)) {
          reader_->io_failure(operation::read);
          backend().del(operation::read, sock_.fd(), this);
        } else if (rb > 0) {
          collected_ += rb;
          if (collected_ >= threshold_) {
            reader_->consume(rd_buf_.data(), collected_);
            read_loop();
          }
        }
        break;
      }
      case operation::write: {
        size_t wb; // written bytes
        if (! write_some(wb, sock_.fd(),
                wr_buf_.data() + written_,
                wr_buf_.size() - written_)) {
          writer_->io_failure(operation::write);
          backend().del(operation::write, sock_.fd(), this);
        }
        else if (wb > 0) {
          written_ += wb;
          if (written_ >= wr_buf_.size()) {
            // prepare next send (or stop sending)
            write_loop();
          }
        }
        break;
      }
      case operation::propagate_error:
        if (reader_) {
          reader_->io_failure(operation::read);
        }
        if (writer_) {
          writer_->io_failure(operation::write);
        }
        // backend will delete this handler anyway,
        // no need to call backend().del() here
        break;
    }
  }

  native_socket fd() const override {
    return sock_.fd();
  }

private:
  void read_loop() {
    collected_ = 0;
    switch (rd_flag_) {
      case receive_policy_flag::exactly:
        if (rd_buf_.size() != max_) {
          rd_buf_.resize(max_);
        }
        threshold_ = max_;
        break;
      case receive_policy_flag::at_most:
        if (rd_buf_.size() != max_) {
          rd_buf_.resize(max_);
        }
        threshold_ = 1;
        break;
      case receive_policy_flag::at_least: {
        // read up to 10% more, but at least allow 100 bytes more
        auto max_size =   max_
                + std::max<size_t>(100, max_ / 10);
        if (rd_buf_.size() != max_size) {
          rd_buf_.resize(max_size);
        }
        threshold_ = max_;
        break;
      }
    }
  }

  void write_loop() {
    CAF_LOG_TRACE("wr_buf size: " << wr_buf_.size()
             << ", offline buf size: " << wr_offline_buf_.size());
    written_ = 0;
    wr_buf_.clear();
    if (wr_offline_buf_.empty()) {
      writing_ = false;
      backend().del(operation::write, sock_.fd(), this);
    } else {
      wr_buf_.swap(wr_offline_buf_);
    }
  }

  // reading & writing
  Socket sock_;
  // reading
  manager_ptr reader_;
  size_t threshold_;
  size_t collected_;
  size_t max_;
  receive_policy_flag rd_flag_;
  buffer_type rd_buf_;
  // writing
  manager_ptr writer_;
  bool writing_;
  size_t written_;
  buffer_type wr_buf_;
  buffer_type wr_offline_buf_;
};

/// An acceptor is responsible for accepting incoming connections.
template <class SocketAcceptor>
class acceptor : public event_handler {
public:
  using socket_type = typename SocketAcceptor::socket_type;

  /// A manager providing the `accept` member function.
  using manager_type = acceptor_manager;

  /// A smart pointer to an acceptor manager.
  using manager_ptr = intrusive_ptr<manager_type>;

  acceptor(default_multiplexer& backend_ref)
      : event_handler(backend_ref),
        accept_sock_(backend_ref),
        sock_(backend_ref) {
    // nop
  }

  /// Returns the IO socket.
  SocketAcceptor& socket_handle() {
    return accept_sock_;
  }

  /// Returns the accepted socket. This member function should
  /// be called only from the `new_connection` callback.
  socket_type& accepted_socket() {
    return sock_;
  }

  /// Initializes this acceptor, setting the socket handle to `fd`.
  void init(SocketAcceptor sock) {
    CAF_LOG_TRACE("sock.fd = " << sock.fd());
    accept_sock_ = std::move(sock);
  }

  /// Starts this acceptor, forwarding all incoming connections to
  /// `manager`. The intrusive pointer will be released after the
  /// acceptor has been closed or an IO error occured.
  void start(const manager_ptr& mgr) {
    CAF_LOG_TRACE("accept_sock_.fd = " << accept_sock_.fd());
    CAF_ASSERT(mgr != nullptr);
    mgr_ = mgr;
    backend().add(operation::read, accept_sock_.fd(), this);
  }

  /// Closes the network connection, thus stopping this acceptor.
  void stop_reading() {
    CAF_LOG_TRACE("accept_sock_.fd = " << accept_sock_.fd()
             << ", mgr_ = " << mgr_.get());
    backend().del(operation::read, accept_sock_.fd(), this);
    accept_sock_.close_read();
  }

  void handle_event(operation op) override {
    CAF_LOG_TRACE("accept_sock_.fd = " << accept_sock_.fd()
             << ", op = " << static_cast<int>(op));
    if (mgr_ && op == operation::read) {
      native_socket sockfd = invalid_native_socket;
      if (try_accept(sockfd, accept_sock_.fd())) {
        if (sockfd != invalid_native_socket) {
          sock_ = socket_type{backend(), sockfd};
          mgr_->new_connection();
        }
      }
    }
  }

  void removed_from_loop(operation op) override {
    CAF_LOG_TRACE("accept_sock_.fd = " << accept_sock_.fd()
             << "op = " << static_cast<int>(op));
    if (op == operation::read) {
      mgr_.reset();
    }
  }

  native_socket fd() const override {
    return accept_sock_.fd();
  }

private:
  manager_ptr mgr_;
  SocketAcceptor accept_sock_;
  socket_type sock_;
};

native_socket new_tcp_connection_impl(const std::string&, uint16_t,
                                      optional<protocol> preferred = none);

default_socket new_tcp_connection(const std::string& host, uint16_t port);

std::pair<native_socket, uint16_t>
new_tcp_acceptor_impl(uint16_t port, const char* addr, bool reuse_addr);

std::pair<default_socket_acceptor, uint16_t>
new_tcp_acceptor(uint16_t port, const char* addr = nullptr,
                 bool reuse_addr = false);

} // namespace network
} // namespace io
} // namespace caf

#endif // CAF_IO_NETWORK_DEFAULT_MULTIPLEXER_HPP
