/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

#include "caf/mixin/memory_cached.hpp"

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
#   include <w32api.h>
#   undef _WIN32_WINNT
#   undef WINVER
#   define _WIN32_WINNT WindowsVista
#   define WINVER WindowsVista
#   include <ws2tcpip.h>
#   include <winsock2.h>
#   include <ws2ipdef.h>
#else
#   include <unistd.h>
#   include <errno.h>
#   include <sys/socket.h>
#endif

// poll vs epoll backend
#if !defined(CAF_LINUX) || defined(CAF_POLL_IMPL) // poll() multiplexer
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

// poll vs epoll backend
#if !defined(CAF_LINUX) || defined(CAF_POLL_IMPL) // poll() multiplexer
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

/**
 * Platform-specific native acceptor socket type.
 */
using native_socket_acceptor = native_socket;

inline int64_t int64_from_native_socket(native_socket sock) {
  // on Windows, SOCK is an unsigned value;
  // hence, static_cast<> alone would yield the wrong result,
  // as our io_handle assumes -1 as invalid value
  return sock == invalid_native_socket ? -1 : static_cast<int64_t>(sock);
}

/**
 * Returns the last socket error as human-readable string.
 */
std::string last_socket_error_as_string();

/**
 * Sets fd to nonblocking if `set_nonblocking == true`
 * or to blocking if `set_nonblocking == false`
 * throws `network_error` on error
 */
void nonblocking(native_socket fd, bool new_value);

/**
 * Creates two connected sockets. The former is the read handle
 * and the latter is the write handle.
 */
std::pair<native_socket, native_socket> create_pipe();

/**
 * Throws network_error with given error message and
 * the platform-specific error code if `add_errno` is `true`.
 */
void throw_io_failure(const char* what, bool add_errno = true);

/**
 * Returns true if `fd` is configured as nodelay socket.
 * @throws network_error
 */
void tcp_nodelay(native_socket fd, bool new_value);

/**
 * Throws `network_error` if `result` is invalid.
 */
void handle_write_result(ssize_t result);

/**
 * Throws `network_error` if `result` is invalid.
 */
void handle_read_result(ssize_t result);

/**
 * Reads up to `len` bytes from `fd,` writing the received data
 * to `buf`. Returns `true` as long as `fd` is readable and `false`
 * if the socket has been closed or an IO error occured. The number
 * of read bytes is stored in `result` (can be 0).
 */
bool read_some(size_t& result, native_socket fd, void* buf, size_t len);

/**
 * Writes up to `len` bytes from `buf` to `fd`.
 * Returns `true` as long as `fd` is readable and `false`
 * if the socket has been closed or an IO error occured. The number
 * of written bytes is stored in `result` (can be 0).
 */
bool write_some(size_t& result, native_socket fd, const void* buf, size_t len);

/**
 * Tries to accept a new connection from `fd`. On success,
 * the new connection is stored in `result`. Returns true
 * as long as
 */
bool try_accept(native_socket& result, native_socket fd);

class default_multiplexer;

/**
 * A socket IO event handler.
 */
class event_handler {
 public:
  event_handler(default_multiplexer& dm);

  virtual ~event_handler();

  /**
   * Returns true once the requested operation is done, i.e.,
   * to signalize the multiplexer to remove this handler.
   * The handler remains in the event loop as long as it returns false.
   */
  virtual void handle_event(operation op) = 0;

  /**
   * Callback to signalize that this handler has been removed
   * from the event loop for operations of type `op`.
   */
  virtual void removed_from_loop(operation op) = 0;

  /**
   * Returns the `multiplexer` this acceptor belongs to.
   */
  inline default_multiplexer& backend() {
    return m_backend;
  }

  /**
   * Returns the bit field storing the subscribed events.
   */
  inline int eventbf() const {
    return m_eventbf;
  }

  /**
   * Sets the bit field storing the subscribed events.
   */
  inline void eventbf(int eventbf) {
    m_eventbf = eventbf;
  }

  /**
   * Returns the native socket handle for this handler.
   */
  virtual native_socket fd() const = 0;

 protected:
  default_multiplexer& m_backend;
  int m_eventbf;
};

/**
 * Low-level socket type used as default.
 */
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
    return m_fd;
  }

  inline native_socket native_handle() const {
    return m_fd;
  }

  inline default_multiplexer& backend() {
    return m_parent;
  }

 private:
  default_multiplexer& m_parent;
  native_socket m_fd;
};

/**
 * Low-level socket type used as default.
 */
using default_socket_acceptor = default_socket;

class default_multiplexer : public multiplexer {

  friend class io::middleman; // disambiguate reference
  friend class supervisor;

 public:

  struct event {
    native_socket fd;
    int mask;
    event_handler* ptr;
  };

  connection_handle add_tcp_scribe(broker*, default_socket&& sock);

  connection_handle add_tcp_scribe(broker*, native_socket fd) override;

  connection_handle add_tcp_scribe(broker*, const std::string& h,
                                    uint16_t port) override;

  accept_handle add_tcp_doorman(broker*, default_socket&& sock);

  accept_handle add_tcp_doorman(broker*, native_socket fd) override;

  accept_handle add_tcp_doorman(broker*, uint16_t p, const char* h) override;

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
    CAF_REQUIRE(fd != invalid_native_socket);
    CAF_REQUIRE(ptr != nullptr || fd == m_pipe.first);
    // the only valid input where ptr == nullptr is our pipe
    // read handle which is only registered for reading
    auto old_bf = ptr ? ptr->eventbf() : input_mask;
    //auto bf = fun(op, old_bf);
    CAF_LOG_TRACE(CAF_TARG(op, static_cast<int>)
             << ", " << CAF_ARG(fd) << ", " CAF_ARG(ptr)
             << ", " << CAF_ARG(old_bf));
    auto event_less = [](const event& e, native_socket fd) -> bool {
      return e.fd < fd;
    };
    auto last = m_events.end();
    auto i = std::lower_bound(m_events.begin(), last, fd, event_less);
    if (i != last && i->fd == fd) {
      CAF_REQUIRE(ptr == i->ptr);
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
        m_events.erase(i);
      }
    } else {
      // insert new element
      auto bf = fun(op, old_bf);
      if (bf == old_bf) {
        CAF_LOG_DEBUG("event has no effect (discarded): "
                 << CAF_ARG(bf) << ", " << CAF_ARG(old_bf));
      } else {
        m_events.insert(i, event{fd, bf, ptr});
      }
    }
  }

  void handle(const event& event);

  void handle_socket_event(native_socket fd, int mask, event_handler* ptr);

  void close_pipe();

  void wr_dispatch_request(runnable* ptr);

  runnable* rd_dispatch_request();

  native_socket m_epollfd; // unused in poll() implementation
  std::vector<multiplexer_data> m_pollset;
  std::vector<event> m_events; // always sorted by .fd
  multiplexer_poll_shadow_data m_shadow;
  std::pair<native_socket, native_socket> m_pipe;

  std::thread::id m_tid;

};

default_multiplexer& get_multiplexer_singleton();

template <class T>
inline connection_handle conn_hdl_from_socket(const T& sock) {
  return connection_handle::from_int(
        int64_from_native_socket(sock.native_handle()));
}

template <class T>
inline accept_handle accept_hdl_from_socket(const T& sock) {
  return accept_handle::from_int(
        int64_from_native_socket(sock.native_handle()));
}


/**
 * A stream capable of both reading and writing. The stream's input
 * data is forwarded to its {@link stream_manager manager}.
 */
template <class Socket>
class stream : public event_handler {
 public:
  /**
   * A smart pointer to a stream manager.
   */
  using manager_ptr = intrusive_ptr<stream_manager>;

  /**
   * A buffer class providing a compatible
   * interface to `std::vector`.
   */
  using buffer_type = std::vector<char>;

  stream(default_multiplexer& backend)
      : event_handler(backend),
        m_sock(backend),
        m_writing(false) {
    configure_read(receive_policy::at_most(1024));
  }

  /**
   * Returns the `multiplexer` this stream belongs to.
   */
  inline default_multiplexer& backend() {
    return static_cast<default_multiplexer&>(m_sock.backend());
  }

  /**
   * Returns the IO socket.
   */
  inline Socket& socket_handle() {
    return m_sock;
  }

  /**
   * Initializes this stream, setting the socket handle to `fd`.
   */
  void init(Socket fd) {
    m_sock = std::move(fd);
  }

  /**
   * Starts reading data from the socket, forwarding incoming data to `mgr`.
   */
  void start(const manager_ptr& mgr) {
    CAF_REQUIRE(mgr != nullptr);
    m_reader = mgr;
    backend().add(operation::read, m_sock.fd(), this);
    read_loop();
  }

  void removed_from_loop(operation op) override {
    switch (op) {
      case operation::read:  m_reader.reset(); break;
      case operation::write: m_writer.reset(); break;
      default: break;
    }
  }

  /**
   * Configures how much data will be provided
   * for the next `consume` callback.
   * @warning Must not be called outside the IO multiplexers event loop
   *          once the stream has been started.
   */
  void configure_read(receive_policy::config config) {
    m_rd_flag = config.first;
    m_max = config.second;
  }

  /**
   * Copies data to the write buffer.
   * @note Not thread safe.
   */
  void write(const void* buf, size_t num_bytes) {
    CAF_LOG_TRACE("num_bytes: " << num_bytes);
    auto first = reinterpret_cast<const char*>(buf);
    auto last  = first + num_bytes;
    m_wr_offline_buf.insert(m_wr_offline_buf.end(), first, last);
  }

  /**
   * Returns the write buffer of this stream.
   * @warning Must not be modified outside the IO multiplexers event loop
   *          once the stream has been started.
   */
  buffer_type& wr_buf() {
    return m_wr_offline_buf;
  }

  buffer_type& rd_buf() {
    return m_rd_buf;
  }

  /**
   * Sends the content of the write buffer, calling the `io_failure`
   * member function of `mgr` in case of an error.
   * @warning Must not be called outside the IO multiplexers event loop
   *          once the stream has been started.
   */
  void flush(const manager_ptr& mgr) {
    CAF_REQUIRE(mgr != nullptr);
    CAF_LOG_TRACE("offline buf size: " << m_wr_offline_buf.size()
             << ", mgr = " << mgr.get()
             << ", m_writer = " << m_writer.get());
    if (!m_wr_offline_buf.empty() && !m_writing) {
      backend().add(operation::write, m_sock.fd(), this);
      m_writer = mgr;
      m_writing = true;
      write_loop();
    }
  }

  void stop_reading() {
    CAF_LOGM_TRACE("caf::io::network::stream", "");
    m_sock.close_read();
    backend().del(operation::read, m_sock.fd(), this);
  }

  void handle_event(operation op) {
    CAF_LOG_TRACE("op = " << static_cast<int>(op));
    switch (op) {
      case operation::read: {
        size_t rb; // read bytes
        if (!read_some(rb, m_sock.fd(),
                 m_rd_buf.data() + m_collected,
                 m_rd_buf.size() - m_collected)) {
          m_reader->io_failure(operation::read);
          backend().del(operation::read, m_sock.fd(), this);
        } else if (rb > 0) {
          m_collected += rb;
          if (m_collected >= m_threshold) {
            m_reader->consume(m_rd_buf.data(), m_collected);
            read_loop();
          }
        }
        break;
      }
      case operation::write: {
        size_t wb; // written bytes
        if (!write_some(wb, m_sock.fd(),
                m_wr_buf.data() + m_written,
                m_wr_buf.size() - m_written)) {
          m_writer->io_failure(operation::write);
          backend().del(operation::write, m_sock.fd(), this);
        }
        else if (wb > 0) {
          m_written += wb;
          if (m_written >= m_wr_buf.size()) {
            // prepare next send (or stop sending)
            write_loop();
          }
        }
        break;
      }
      case operation::propagate_error:
        if (m_reader) {
          m_reader->io_failure(operation::read);
        }
        if (m_writer) {
          m_writer->io_failure(operation::write);
        }
        // backend will delete this handler anyway,
        // no need to call backend().del() here
        break;
    }
  }

 protected:
  native_socket fd() const override {
    return m_sock.fd();
  }

 private:
  void read_loop() {
    m_collected = 0;
    switch (m_rd_flag) {
      case receive_policy_flag::exactly:
        if (m_rd_buf.size() != m_max) {
          m_rd_buf.resize(m_max);
        }
        m_threshold = m_max;
        break;
      case receive_policy_flag::at_most:
        if (m_rd_buf.size() != m_max) {
          m_rd_buf.resize(m_max);
        }
        m_threshold = 1;
        break;
      case receive_policy_flag::at_least: {
        // read up to 10% more, but at least allow 100 bytes more
        auto max_size =   m_max
                + std::max<size_t>(100, m_max / 10);
        if (m_rd_buf.size() != max_size) {
          m_rd_buf.resize(max_size);
        }
        m_threshold = m_max;
        break;
      }
    }
  }

  void write_loop() {
    CAF_LOG_TRACE("wr_buf size: " << m_wr_buf.size()
             << ", offline buf size: " << m_wr_offline_buf.size());
    m_written = 0;
    m_wr_buf.clear();
    if (m_wr_offline_buf.empty()) {
      m_writing = false;
      backend().del(operation::write, m_sock.fd(), this);
    } else {
      m_wr_buf.swap(m_wr_offline_buf);
    }
  }

  // reading & writing
  Socket        m_sock;
  // reading
  manager_ptr     m_reader;
  size_t        m_threshold;
  size_t        m_collected;
  size_t        m_max;
  receive_policy_flag m_rd_flag;
  buffer_type     m_rd_buf;
  // writing
  manager_ptr     m_writer;
  bool        m_writing;
  size_t        m_written;
  buffer_type     m_wr_buf;
  buffer_type     m_wr_offline_buf;
};

/**
 * An acceptor is responsible for accepting incoming connections.
 */
template <class SocketAcceptor>
class acceptor : public event_handler {

 public:

  using socket_type = typename SocketAcceptor::socket_type;

  /**
   * A manager providing the `accept` member function.
   */
  using manager_type = acceptor_manager;

  /**
   * A smart pointer to an acceptor manager.
   */
  using manager_ptr = intrusive_ptr<manager_type>;

  acceptor(default_multiplexer& backend)
      : event_handler(backend),
        m_accept_sock(backend),
        m_sock(backend) {
    // nop
  }

  /**
   * Returns the IO socket.
   */
  inline SocketAcceptor& socket_handle() {
    return m_accept_sock;
  }

  /**
   * Returns the accepted socket. This member function should
   * be called only from the `new_connection` callback.
   */
  inline socket_type& accepted_socket() {
    return m_sock;
  }

  /**
   * Initializes this acceptor, setting the socket handle to `fd`.
   */
  void init(SocketAcceptor sock) {
    CAF_LOG_TRACE("sock.fd = " << sock.fd());
    m_accept_sock = std::move(sock);
  }

  /**
   * Starts this acceptor, forwarding all incoming connections to
   * `manager`. The intrusive pointer will be released after the
   * acceptor has been closed or an IO error occured.
   */
  void start(const manager_ptr& mgr) {
    CAF_LOG_TRACE("m_accept_sock.fd = " << m_accept_sock.fd());
    CAF_REQUIRE(mgr != nullptr);
    m_mgr = mgr;
    backend().add(operation::read, m_accept_sock.fd(), this);
  }

  /**
   * Closes the network connection, thus stopping this acceptor.
   */
  void stop_reading() {
    CAF_LOG_TRACE("m_accept_sock.fd = " << m_accept_sock.fd()
             << ", m_mgr = " << m_mgr.get());
    backend().del(operation::read, m_accept_sock.fd(), this);
    m_accept_sock.close_read();
  }

  void handle_event(operation op) override {
    CAF_LOG_TRACE("m_accept_sock.fd = " << m_accept_sock.fd()
             << ", op = " << static_cast<int>(op));
    if (m_mgr && op == operation::read) {
      native_socket fd = invalid_native_socket;
      if (try_accept(fd, m_accept_sock.fd())) {
        if (fd != invalid_native_socket) {
          m_sock = socket_type{backend(), fd};
          m_mgr->new_connection();
        }
      }
    }
  }

  void removed_from_loop(operation op) override {
    CAF_LOG_TRACE("m_accept_sock.fd = " << m_accept_sock.fd()
             << "op = " << static_cast<int>(op));
    if (op == operation::read) m_mgr.reset();
  }

 protected:

  native_socket fd() const override {
    return m_accept_sock.fd();
  }

 private:

  manager_ptr    m_mgr;
  SocketAcceptor m_accept_sock;
  socket_type    m_sock;

};

native_socket new_ipv4_connection_impl(const std::string&, uint16_t);

default_socket new_ipv4_connection(const std::string& host, uint16_t port);

template <class Socket>
void ipv4_connect(Socket& sock, const std::string& host, uint16_t port) {
  sock = new_ipv4_connection(host, port);
}

native_socket new_ipv4_acceptor_impl(uint16_t port, const char* addr);

default_socket_acceptor new_ipv4_acceptor(uint16_t port,
                      const char* addr = nullptr);

template <class SocketAcceptor>
void ipv4_bind(SocketAcceptor& sock,
         uint16_t port,
         const char* addr = nullptr) {
  CAF_LOGF_TRACE(CAF_ARG(port));
  sock = new_ipv4_acceptor(port, addr);
}

} // namespace network
} // namespace io
} // namespace caf

#endif // CAF_IO_NETWORK_DEFAULT_MULTIPLEXER_HPP
