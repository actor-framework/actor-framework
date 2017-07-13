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

#ifndef CAF_IO_NETWORK_DEFAULT_MULTIPLEXER_HPP
#define CAF_IO_NETWORK_DEFAULT_MULTIPLEXER_HPP

#include <thread>

#include <vector>
#include <string>
#include <cstdint>

#include "caf/unit.hpp"
#include "caf/config.hpp"
#include "caf/extend.hpp"
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

#include "caf/logger.hpp"

#ifdef CAF_WINDOWS
# ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
# endif // WIN32_LEAN_AND_MEAN
# ifndef NOMINMAX
#   define NOMINMAX
# endif
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
# include <cerrno>
# include <sys/socket.h>
#endif

// kqueue vs poll vs epoll backend
#if defined(CAF_MACOS) || defined(CAF_BSD) // kqueue() multiplexer
# define CAF_KQUEUE_MULTIPLEXER
# include <sys/event.h>
#elif !defined(CAF_LINUX) || defined(CAF_POLL_IMPL) // poll() multiplexer
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

// kqueue vs poll vs epoll backend
#if defined(CAF_MACOS) || defined(CAF_BSD)
  constexpr int input_mask  = 0x01;
  constexpr int output_mask = 0x02;
  constexpr int error_mask  = 0x04;
  // we use the shadow data to store the changeset for kqueue
  using multiplexer_data = struct kevent;
  class event_handler;
  struct multiplexer_poll_shadow_data {
    std::vector<struct kevent> changes; // changelist passed to kevent()
    std::vector<native_socket> fds;  // registered fds
  };
#elif !defined(CAF_LINUX) || defined(CAF_POLL_IMPL) // poll() multiplexer
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

/// Creates two connected sockets. The former is the read handle
/// and the latter is the write handle.
std::pair<native_socket, native_socket> create_pipe();

/// Sets fd to nonblocking if `set_nonblocking == true`
/// or to blocking if `set_nonblocking == false`
/// throws `network_error` on error
expected<void> nonblocking(native_socket fd, bool new_value);

/// Enables or disables Nagle's algorithm on `fd`.
/// @throws network_error
expected<void> tcp_nodelay(native_socket fd, bool new_value);

/// Enables or disables `SIGPIPE` events from `fd`.
expected<void> allow_sigpipe(native_socket fd, bool new_value);

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

/// Returns the locally assigned port of `fd`.
expected<uint16_t> local_port_of_fd(native_socket fd);

/// Returns the locally assigned address of `fd`.
expected<std::string> local_addr_of_fd(native_socket fd);

/// Returns the port used by the remote host of `fd`.
expected<uint16_t> remote_port_of_fd(native_socket fd);

/// Returns the remote host address of `fd`.
expected<std::string> remote_addr_of_fd(native_socket fd);

class default_multiplexer;

/// A socket I/O event handler.
class event_handler {
public:
  event_handler(default_multiplexer& dm, native_socket sockfd);

  virtual ~event_handler();

  /// Returns true once the requested operation is done, i.e.,
  /// to signalize the multiplexer to remove this handler.
  /// The handler remains in the event loop as long as it returns false.
  virtual void handle_event(operation op) = 0;

  /// Callback to signalize that this handler has been removed
  /// from the event loop for operations of type `op`.
  virtual void removed_from_loop(operation op) = 0;

  /// Returns the native socket handle for this handler.
  inline native_socket fd() const {
    return fd_;
  }

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

  /// Checks whether `close_read` has been called.
  inline bool read_channel_closed() const {
    return read_channel_closed_;
  }

  /// Closes the read channel of the underlying socket.
  void close_read_channel();

  /// Removes the file descriptor from the event loop of the parent.
  void passivate();

protected:
  /// Adds the file descriptor to the event loop of the parent.
  void activate();

  void set_fd_flags();

  int eventbf_;
  native_socket fd_;
  bool read_channel_closed_;
  default_multiplexer& backend_;
};

/// An event handler for the internal event pipe.
class pipe_reader : public event_handler {
public:
  pipe_reader(default_multiplexer& dm);
  void removed_from_loop(operation op) override;
  void handle_event(operation op) override;
  void init(native_socket sock_fd);
  resumable* try_read_next();
};

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

  scribe_ptr new_scribe(native_socket fd) override;

  expected<scribe_ptr> new_tcp_scribe(const std::string& host,
                                      uint16_t port) override;

  doorman_ptr new_doorman(native_socket fd) override;

  expected<doorman_ptr> new_tcp_doorman(uint16_t port, const char* in,
                                        bool reuse_addr) override;

  void exec_later(resumable* ptr) override;

  explicit default_multiplexer(actor_system* sys);

  ~default_multiplexer() override;

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
    CAF_LOG_TRACE(CAF_ARG(op) << CAF_ARG(fd) << CAF_ARG(old_bf));
    auto last = events_.end();
    auto i = std::lower_bound(events_.begin(), last, fd, event_less{});
    if (i != last && i->fd == fd) {
      CAF_ASSERT(ptr == i->ptr);
      // squash events together
      CAF_LOG_DEBUG("squash events:" << CAF_ARG(i->mask)
                    << CAF_ARG(fun(op, i->mask)));
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
        CAF_LOG_DEBUG("added handler:" << CAF_ARG(fd) << CAF_ARG(op));
        events_.insert(i, event{fd, bf, ptr});
      }
    }
  }

  void handle(const event& e);

  void handle_socket_event(native_socket fd, int mask, event_handler* ptr);

  void close_pipe();

  void wr_dispatch_request(resumable* ptr);

  //resumable* rd_dispatch_request();

  native_socket loopfd_; // unused in poll() implementation
  std::vector<multiplexer_data> pollset_;
  std::vector<event> events_; // always sorted by .fd
  multiplexer_poll_shadow_data shadow_;
  std::pair<native_socket, native_socket> pipe_;
  pipe_reader pipe_reader_;
};

inline connection_handle conn_hdl_from_socket(native_socket fd) {
  return connection_handle::from_int(int64_from_native_socket(fd));
}

inline accept_handle accept_hdl_from_socket(native_socket fd) {
  return accept_handle::from_int(int64_from_native_socket(fd));
}

/// A stream capable of both reading and writing. The stream's input
/// data is forwarded to its {@link stream_manager manager}.
class stream : public event_handler {
public:
  /// A smart pointer to a stream manager.
  using manager_ptr = intrusive_ptr<stream_manager>;

  /// A buffer class providing a compatible
  /// interface to `std::vector`.
  using buffer_type = std::vector<char>;

  stream(default_multiplexer& backend_ref, native_socket sockfd);

  /// Starts reading data from the socket, forwarding incoming data to `mgr`.
  void start(stream_manager* mgr);

  /// Activates the stream.
  void activate(stream_manager* mgr);

  /// Configures how much data will be provided for the next `consume` callback.
  /// @warning Must not be called outside the IO multiplexers event loop
  ///          once the stream has been started.
  void configure_read(receive_policy::config config);

  void ack_writes(bool x);

  /// Copies data to the write buffer.
  /// @warning Not thread safe.
  void write(const void* buf, size_t num_bytes);

  /// Returns the write buffer of this stream.
  /// @warning Must not be modified outside the IO multiplexers event loop
  ///          once the stream has been started.
  inline buffer_type& wr_buf() {
    return wr_offline_buf_;
  }

  /// Returns the read buffer of this stream.
  /// @warning Must not be modified outside the IO multiplexers event loop
  ///          once the stream has been started.
  inline buffer_type& rd_buf() {
    return rd_buf_;
  }

  /// Sends the content of the write buffer, calling the `io_failure`
  /// member function of `mgr` in case of an error.
  /// @warning Must not be called outside the IO multiplexers event loop
  ///          once the stream has been started.
  void flush(const manager_ptr& mgr);

  /// Closes the read channel of the underlying socket and removes
  /// this handler from its parent.
  void stop_reading();

  void removed_from_loop(operation op) override;

protected:
  template <class Policy>
  void handle_event_impl(io::network::operation op, Policy& policy) {
    CAF_LOG_TRACE(CAF_ARG(op));
    auto mcr = max_consecutive_reads();
    switch (op) {
      case io::network::operation::read: {
        // Loop until an error occurs or we have nothing more to read
        // or until we have handled `mcr` reads.
        size_t rb;
        for (size_t i = 0; i < mcr; ++i) {
          if (!policy.read_some(rb, fd(), rd_buf_.data() + collected_,
                                rd_buf_.size() - collected_)) {
            reader_->io_failure(&backend(), operation::read);
            passivate();
            return;
          }
          if (rb == 0)
            return;
          collected_ += rb;
          if (collected_ >= read_threshold_) {
            auto res = reader_->consume(&backend(), rd_buf_.data(), collected_);
            prepare_next_read();
            if (!res) {
              passivate();
              return;
            }
          }
        }
        break;
      }
      case io::network::operation::write: {
        size_t wb; // written bytes
        if (!policy.write_some(wb, fd(), wr_buf_.data() + written_,
                               wr_buf_.size() - written_)) {
          writer_->io_failure(&backend(), operation::write);
          backend().del(operation::write, fd(), this);
        } else if (wb > 0) {
          written_ += wb;
          CAF_ASSERT(written_ <= wr_buf_.size());
          auto remaining = wr_buf_.size() - written_;
          if (ack_writes_)
            writer_->data_transferred(&backend(), wb,
                                      remaining + wr_offline_buf_.size());
          // prepare next send (or stop sending)
          if (remaining == 0)
            prepare_next_write();
        }
        break;
      }
      case operation::propagate_error:
        if (reader_)
          reader_->io_failure(&backend(), operation::read);
        if (writer_)
          writer_->io_failure(&backend(), operation::write);
        // backend will delete this handler anyway,
        // no need to call backend().del() here
    }
  }

private:
  size_t max_consecutive_reads();

  void prepare_next_read();

  void prepare_next_write();

  // state for reading
  manager_ptr reader_;
  size_t read_threshold_;
  size_t collected_;
  size_t max_;
  receive_policy_flag rd_flag_;
  buffer_type rd_buf_;

  // state for writing
  manager_ptr writer_;
  bool ack_writes_;
  bool writing_;
  size_t written_;
  buffer_type wr_buf_;
  buffer_type wr_offline_buf_;
};

/// A concrete stream with a technology-dependent policy for sending and
/// receiving data from a socket.
template <class ProtocolPolicy>
class stream_impl : public stream {
public:
  template <class... Ts>
  stream_impl(default_multiplexer& mpx, native_socket sockfd, Ts&&... xs)
    : stream(mpx, sockfd),
      policy_(std::forward<Ts>(xs)...) {
    // nop
  }

  void handle_event(io::network::operation op) override {
    this->handle_event_impl(op, policy_);
  }

private:
  ProtocolPolicy policy_;
};

/// An acceptor is responsible for accepting incoming connections.
class acceptor : public event_handler {
public:
  /// A manager providing the `accept` member function.
  using manager_type = acceptor_manager;

  /// A smart pointer to an acceptor manager.
  using manager_ptr = intrusive_ptr<manager_type>;

  acceptor(default_multiplexer& backend_ref, native_socket sockfd);

  /// Returns the accepted socket. This member function should
  /// be called only from the `new_connection` callback.
  inline native_socket& accepted_socket() {
    return sock_;
  }

  /// Starts this acceptor, forwarding all incoming connections to
  /// `manager`. The intrusive pointer will be released after the
  /// acceptor has been closed or an IO error occured.
  void start(acceptor_manager* mgr);

  /// Activates the acceptor.
  void activate(acceptor_manager* mgr);

  /// Closes the network connection and removes this handler from its parent.
  void stop_reading();

  void removed_from_loop(operation op) override;

protected:
  template <class Policy>
  void handle_event_impl(io::network::operation op, Policy& policy) {
    CAF_LOG_TRACE(CAF_ARG(fd()) << CAF_ARG(op));
    if (mgr_ && op == operation::read) {
      native_socket sockfd = invalid_native_socket;
      if (policy.try_accept(sockfd, fd())) {
        if (sockfd != invalid_native_socket) {
          sock_ = sockfd;
          mgr_->new_connection();
        }
      }
    }
  }

private:
  manager_ptr mgr_;
  native_socket sock_;
};

/// A concrete acceptor with a technology-dependent policy.
template <class ProtocolPolicy>
class acceptor_impl : public acceptor {
public:
  template <class... Ts>
  acceptor_impl(default_multiplexer& mpx, native_socket sockfd, Ts&&... xs)
    : acceptor(mpx, sockfd),
      policy_(std::forward<Ts>(xs)...) {
    // nop
  }

  void handle_event(io::network::operation op) override {
    this->handle_event_impl(op, policy_);
  }

private:
  ProtocolPolicy policy_;
};

expected<native_socket> new_tcp_connection(const std::string& host,
                                           uint16_t port,
                                           optional<protocol> preferred = none);

expected<native_socket> new_tcp_acceptor_impl(uint16_t port, const char* addr,
                                              bool reuse_addr);

} // namespace network
} // namespace io
} // namespace caf

#endif // CAF_IO_NETWORK_DEFAULT_MULTIPLEXER_HPP
