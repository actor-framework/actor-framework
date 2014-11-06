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

#include "caf/io/network/default_multiplexer.hpp"

#include "caf/config.hpp"
#include "caf/exception.hpp"

#include "caf/io/broker.hpp"
#include "caf/io/middleman.hpp"

#ifdef CAF_WINDOWS
# include <winsock2.h>
# include <ws2tcpip.h> /* socklen_t, et al (MSVC20xx) */
# include <windows.h>
# include <io.h>
#else
# include <errno.h>
# include <netdb.h>
# include <fcntl.h>
# include <sys/types.h>
# include <arpa/inet.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/tcp.h>
#endif

using std::string;

namespace caf {
namespace io {
namespace network {

/******************************************************************************
 *           platform-dependent implementations           *
 ******************************************************************************/

#ifndef CAF_WINDOWS

  string last_socket_error_as_string() {
    return strerror(errno);
  }

  void nonblocking(native_socket fd, bool new_value) {
    // read flags for fd
    auto rf = fcntl(fd, F_GETFL, 0);
    if (rf == -1) {
      throw_io_failure("unable to read socket flags");
    }
    // calculate and set new flags
    auto wf = new_value ? (rf | O_NONBLOCK) : (rf & (~(O_NONBLOCK)));
    if (fcntl(fd, F_SETFL, wf) < 0) {
      throw_io_failure("unable to set file descriptor flags");
    }
  }

  std::pair<native_socket, native_socket> create_pipe() {
    int pipefds[2];
    if (pipe(pipefds) != 0) {
      perror("pipe");
      exit(EXIT_FAILURE);
    }
    return {pipefds[0], pipefds[1]};
  }

#else // CAF_WINDOWS

  string last_socket_error_as_string() {
    LPTSTR errorText = NULL;
    auto hresult = last_socket_error();
    FormatMessage( // use system message tables to retrieve error text
      FORMAT_MESSAGE_FROM_SYSTEM
      // allocate buffer on local heap for error text
      | FORMAT_MESSAGE_ALLOCATE_BUFFER
      // Important! will fail otherwise, since we're not
      // (and CANNOT) pass insertion parameters
      | FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, // unused with FORMAT_MESSAGE_FROM_SYSTEM
      hresult, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPTSTR) & errorText, // output
      0,                    // minimum size for output buffer
      nullptr);             // arguments - see note
    std::string result;
    if (errorText != nullptr) {
      result = errorText;
      // release memory allocated by FormatMessage()
      LocalFree(errorText);
    }
    return result;
  }

  void nonblocking(native_socket fd, bool new_value) {
    u_long mode = new_value ? 1 : 0;
    if (ioctlsocket(fd, FIONBIO, &mode) < 0) {
      throw_io_failure("unable to set FIONBIO");
    }
  }

  // calls a C function and converts its returned error code to an exception
  template <class F, class... Ts>
  inline void ccall(const char* errmsg, F f, Ts&&... args) {
    if (f(std::forward<Ts>(args)...) != 0) {
      throw_io_failure(errmsg);
    }
  }

  /**************************************************************************\
   * Based on work of others;                                               *
   * original header:                                                       *
   *                                                                        *
   * Copyright 2007, 2010 by Nathan C. Myers <ncm@cantrip.org>              *
   * Redistribution and use in source and binary forms, with or without     *
   * modification, are permitted provided that the following conditions     *
   * are met:                                                               *
   *                                                                        *
   * Redistributions of source code must retain the above copyright notice, *
   * this list of conditions and the following disclaimer.                  *
   *                                                                        *
   * Redistributions in binary form must reproduce the above copyright      *
   * notice, this list of conditions and the following disclaimer in the    *
   * documentation and/or other materials provided with the distribution.   *
   *                                                                        *
   * The name of the author must not be used to endorse or promote products *
   * derived from this software without specific prior written permission.  *
   *                                                                        *
   * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS    *
   * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT      *
   * LIMITED  TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR *
   * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT   *
   * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, *
   * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT       *
   * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,  *
   * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY  *
   * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT    *
   * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE  *
   * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.   *
  \**************************************************************************/
  std::pair<native_socket, native_socket> create_pipe() {
    socklen_t addrlen = sizeof(sockaddr_in);
    native_socket socks[2] = {invalid_native_socket, invalid_native_socket};
    auto listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == invalid_native_socket) {
      throw_io_failure("socket() failed");
    }
    union {
      sockaddr_in inaddr;
      sockaddr addr;
    } a;
    memset(&a, 0, sizeof(a));
    a.inaddr.sin_family = AF_INET;
    a.inaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.inaddr.sin_port = 0;
    bool success = false;
    // makes sure all sockets are closed in case of an error
    auto guard = detail::make_scope_guard([&] {
      if (success) {
        return; // everyhting's fine
      }
      auto e = WSAGetLastError();
      closesocket(listener);
      closesocket(socks[0]);
      closesocket(socks[1]);
      WSASetLastError(e);
    });
    // bind listener to a local port
    int reuse = 1;
    ccall("setsockopt() failed", ::setsockopt, listener, SOL_SOCKET,
          SO_REUSEADDR, reinterpret_cast<char*>(&reuse),
          static_cast<socklen_t>(sizeof(reuse)));
    ccall("bind() failed", ::bind, listener, &a.addr, sizeof(a.inaddr));
    // read the port in use: win32 getsockname may only set the port number
    // (http://msdn.microsoft.com/library/ms738543.aspx):
    memset(&a, 0, sizeof(a));
    ccall("getsockname() failed", ::getsockname, listener, &a.addr, &addrlen);
    a.inaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.inaddr.sin_family = AF_INET;
    // set listener to listen mode
    ccall("listen() failed", ::listen, listener, 1);
    // create read-only end of the pipe
    DWORD flags = 0;
    auto read_fd = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, flags);
    if (read_fd == invalid_native_socket) {
      throw_io_failure("cannot create read handle: WSASocket() failed");
    }
    ccall("connect() failed", ::connect, read_fd, &a.addr, sizeof(a.inaddr));
    // get write-only end of the pipe
    auto write_fd = accept(listener, NULL, NULL);
    if (write_fd == invalid_native_socket) {
      throw_io_failure("cannot create write handle: accept() failed");
    }
    closesocket(listener);
    success = true;
    return {read_fd, write_fd};
  }

#endif

/******************************************************************************
 *                             epoll() vs. poll()                             *
 ******************************************************************************/

#ifdef CAF_EPOLL_MULTIPLEXER

  // In this implementation, m_shadow is the number of sockets we have
  // registered to epoll.

  default_multiplexer::default_multiplexer()
      : m_epollfd(invalid_native_socket),
        m_shadow(1) {
    init();
    m_epollfd = epoll_create1(EPOLL_CLOEXEC);
    if (m_epollfd == -1) {
      CAF_LOG_ERROR("epoll_create1: " << strerror(errno));
      exit(errno);
    }
    // handle at most 64 events at a time
    m_pollset.resize(64);
    m_pipe = create_pipe();
    epoll_event ee;
    ee.events = input_mask;
    ee.data.ptr = nullptr;
    if (epoll_ctl(m_epollfd, EPOLL_CTL_ADD, m_pipe.first, &ee) < 0) {
      CAF_LOG_ERROR("epoll_ctl: " << strerror(errno));
      exit(errno);
    }
  }

  void default_multiplexer::run() {
    CAF_LOG_TRACE("epoll()-based multiplexer");
    while (m_shadow > 0) {
      int presult = epoll_wait(m_epollfd, m_pollset.data(),
                               static_cast<int>(m_pollset.size()), -1);
      CAF_LOG_DEBUG("epoll_wait() on " << m_shadow << " sockets reported "
                    << presult << " event(s)");
      if (presult < 0) {
        switch (errno) {
          case EINTR: {
            // a signal was caught
            // just try again
            continue;
          }
          default: {
            perror("epoll_wait() failed");
            CAF_CRITICAL("epoll_wait() failed");
          }
        }
      }
      auto iter = m_pollset.begin();
      auto last = iter + presult;
      for (; iter != last; ++iter) {
        auto ptr = reinterpret_cast<event_handler*>(iter->data.ptr);
        auto fd = ptr ? ptr->fd() : m_pipe.first;
        handle_socket_event(fd, iter->events, ptr);
      }
      for (auto& me : m_events) {
        handle(me);
      }
      m_events.clear();
    }
  }

  void default_multiplexer::handle(const default_multiplexer::event& e) {
    CAF_LOG_TRACE("e.fd = " << e.fd << ", mask = " << e.mask);
    // ptr is only allowed to nullptr if fd is our pipe
    // read handle which is only registered for input
    CAF_REQUIRE(e.ptr != nullptr || e.fd == m_pipe.first);
    if (e.ptr && e.ptr->eventbf() == e.mask) {
      // nop
      return;
    }
    auto old = e.ptr ? e.ptr->eventbf() : input_mask;
    if (e.ptr){
      e.ptr->eventbf(e.mask);
    }
    epoll_event ee;
    ee.events = e.mask;
    ee.data.ptr = e.ptr;
    int op;
    if (e.mask == 0) {
      CAF_LOG_DEBUG("attempt to remove socket " << e.fd << " from epoll");
      op = EPOLL_CTL_DEL;
      --m_shadow;
    } else if (old == 0) {
      CAF_LOG_DEBUG("attempt to add socket " << e.fd << " to epoll");
      op = EPOLL_CTL_ADD;
      ++m_shadow;
    } else {
      CAF_LOG_DEBUG("modify epoll event mask for socket " << e.fd
                    << ": " << old << " -> " << e.mask);
      op = EPOLL_CTL_MOD;
    }
    if (epoll_ctl(m_epollfd, op, e.fd, &ee) < 0) {
      switch (last_socket_error()) {
        // supplied file descriptor is already registered
        case EEXIST:
          CAF_LOG_ERROR("file descriptor registered twice");
          --m_shadow;
          break;
        // op was EPOLL_CTL_MOD or EPOLL_CTL_DEL,
        // and fd is not registered with this epoll instance.
        case ENOENT:
          CAF_LOG_ERROR(
            "cannot delete file descriptor "
            "because it isn't registered");
          if (e.mask == 0) {
            ++m_shadow;
          }
          break;
        default:
          CAF_LOG_ERROR(strerror(errno));
          perror("epoll_ctl() failed");
          CAF_CRITICAL("epoll_ctl() failed");
      }
    }
    if (e.ptr) {
      auto remove_from_loop_if_needed = [&](int flag, operation flag_op) {
        if ((old & flag) && !(e.mask & flag)) {
          e.ptr->removed_from_loop(flag_op);
        }
      };
      remove_from_loop_if_needed(input_mask, operation::read);
      remove_from_loop_if_needed(output_mask, operation::write);
    }
  }

#else // CAF_EPOLL_MULTIPLEXER

  // Let's be honest: the API of poll() sucks. When dealing with 1000 sockets
  // and the very last socket in your pollset triggers, you have to traverse
  // all elements only to find a single event. Even worse, poll() does
  // not give you a way of storing a user-defined pointer in the pollset.
  // Hence, you need to find a pointer to the actual object managing the
  // socket. When using a map, your already dreadful O(n) turns into
  // a worst case of O(n * log n). To deal with this nonsense, we have two
  // vectors in this implementation: m_pollset and m_shadow. The former
  // stores our pollset, the latter stores our pointers. Both vectors
  // are sorted by the file descriptor. This allows us to quickly,
  // i.e., O(1), access the actual object when handling socket events.

  default_multiplexer::default_multiplexer() : m_epollfd(-1) {
    init();
    // initial setup
    m_pipe = create_pipe();
    pollfd pipefd;
    pipefd.fd = m_pipe.first;
    pipefd.events = input_mask;
    pipefd.revents = 0;
    m_pollset.push_back(pipefd);
    m_shadow.push_back(nullptr);
  }

  void default_multiplexer::run() {
    CAF_LOG_TRACE("poll()-based multiplexer; " << CAF_ARG(input_mask)
                  << ", " << CAF_ARG(output_mask)
                  << ", " << CAF_ARG(error_mask));
    // we store the results of poll() in a separate vector , because
    // altering the pollset while traversing it is not exactly a
    // bright idea ...
    struct fd_event {
      native_socket fd;        // our file descriptor
      short           mask;    // the event mask returned by poll()
      short           fd_mask; // the mask associated with fd
      event_handler*  ptr;     // nullptr in case of a pipe event
    };
    std::vector<fd_event> poll_res;
    while (!m_pollset.empty()) {
      int presult;
#     ifdef CAF_WINDOWS
        presult = ::WSAPoll(m_pollset.data(), m_pollset.size(), -1);
#     else
        presult = ::poll(m_pollset.data(),
                         static_cast<nfds_t>(m_pollset.size()), -1);
#     endif
      CAF_LOG_DEBUG("poll() on " << m_pollset.size() << " sockets reported "
                    << presult << " event(s)");
      if (presult < 0) {
        switch (last_socket_error()) {
          case EINTR: {
            // a signal was caught
            // just try again
            break;
          }
          case ENOMEM: {
            CAF_LOG_ERROR("poll() failed for reason ENOMEM");
            // there's not much we can do other than try again
            // in hope someone else releases memory
            break;
          }
          default: {
            perror("poll() failed");
            CAF_CRITICAL("poll() failed");
          }
        }
        continue; // rince and repeat
      }
      // scan pollset for events first, because we might alter m_pollset
      // while running callbacks (not a good idea while traversing it)
      for (size_t i = 0; i < m_pollset.size() && presult > 0; ++i) {
        auto& pfd = m_pollset[i];
        if (pfd.revents != 0) {
          CAF_LOG_DEBUG("event on socket " << pfd.fd
                        << ", revents = " << pfd.revents);
          poll_res.push_back({pfd.fd, pfd.revents, pfd.events, m_shadow[i]});
          pfd.revents = 0;
          --presult; // stop as early as possible
        }
      }
      for (auto& e : poll_res) {
        // we try to read/write as much as possible by ignoring
        // error states as long as there are still valid
        // operations possible on the socket
        handle_socket_event(e.fd, e.mask, e.ptr);
      }
      poll_res.clear();
      for (auto& me : m_events) {
        handle(me);
      }
      m_events.clear();
    }
  }

  void default_multiplexer::handle(const default_multiplexer::event& e) {
    CAF_REQUIRE(e.fd != invalid_native_socket);
    CAF_REQUIRE(m_pollset.size() == m_shadow.size());
    CAF_LOGF_TRACE("fd = " << e.fd
            << ", old mask = " << (e.ptr ? e.ptr->eventbf() : -1)
            << ", new mask = " << e.mask);
    using namespace std;
    auto last = m_pollset.end();
    auto i = std::lower_bound(m_pollset.begin(), last, e.fd,
                              [](const pollfd& lhs, int rhs) {
                                return lhs.fd < rhs;
                              });
    pollfd new_element;
    new_element.fd = e.fd;
    new_element.events = static_cast<short>(e.mask);
    new_element.revents = 0;
    int old_mask = 0;
    if (e.ptr) {
      old_mask = e.ptr->eventbf();
      e.ptr->eventbf(e.mask);
    }
    // calculate shadow of i
    multiplexer_poll_shadow_data::iterator j;
    if (i == last) {
      j = m_shadow.end();
    } else {
      j = m_shadow.begin();
      std::advance(j, distance(m_pollset.begin(), i));
    }
    // modify vectors
    if (i == last) { // append
      if (e.mask != 0) {
        m_pollset.push_back(new_element);
        m_shadow.push_back(e.ptr);
      }
    } else if (i->fd == e.fd) { // modify
      if (e.mask == 0) {
        // delete item
        m_pollset.erase(i);
        m_shadow.erase(j);
      } else {
        // update event mask of existing entry
        CAF_REQUIRE(*j == e.ptr);
        i->events = e.mask;
      }
      if (e.ptr) {
        auto remove_from_loop_if_needed = [&](int flag, operation flag_op) {
          if ((old_mask & flag) && !(e.mask & flag)) {
            e.ptr->removed_from_loop(flag_op);
          }
        };
        remove_from_loop_if_needed(input_mask, operation::read);
        remove_from_loop_if_needed(output_mask, operation::write);
      }
    } else { // insert at iterator pos
      m_pollset.insert(i, new_element);
      m_shadow.insert(j, e.ptr);
    }
  }

#endif // CAF_EPOLL_MULTIPLEXER

int add_flag(operation op, int bf) {
  switch (op) {
    case operation::read:
      return bf | input_mask;
    case operation::write:
      return bf | output_mask;
    default:
      CAF_LOGF_ERROR("unexpected operation");
      break;
  }
  // weird stuff going on
  return 0;
}

int del_flag(operation op, int bf) {
  switch (op) {
    case operation::read:
      return bf & ~input_mask;
    case operation::write:
      return bf & ~output_mask;
    default:
      CAF_LOGF_ERROR("unexpected operation");
      break;
  }
  // weird stuff going on
  return 0;
}

void default_multiplexer::add(operation op, native_socket fd, event_handler* ptr) {
  CAF_REQUIRE(fd != invalid_native_socket);
  // ptr == nullptr is only allowed to store our pipe read handle
  // and the pipe read handle is added in the ctor (not allowed here)
  CAF_REQUIRE(ptr != nullptr);
  CAF_LOG_TRACE(CAF_TARG(op, static_cast<int>)<< ", " << CAF_ARG(fd)
                                              << ", " CAF_ARG(ptr));
  new_event(add_flag, op, fd, ptr);
}

void default_multiplexer::del(operation op, native_socket fd, event_handler* ptr) {
  CAF_REQUIRE(fd != invalid_native_socket);
  // ptr == nullptr is only allowed when removing our pipe read handle
  CAF_REQUIRE(ptr != nullptr || fd == m_pipe.first);
  CAF_LOG_TRACE(CAF_TARG(op, static_cast<int>)<< ", " << CAF_ARG(fd)
                                              << ", " CAF_ARG(ptr));
  new_event(del_flag, op, fd, ptr);
}

void default_multiplexer::wr_dispatch_request(runnable* ptr) {
  intptr_t ptrval = reinterpret_cast<intptr_t>(ptr);
  // on windows, we actually have sockets, otherwise we have file handles
# ifdef CAF_WINDOWS
    ::send(m_pipe.second, reinterpret_cast<socket_send_ptr>(&ptrval),
           sizeof(ptrval), 0);
# else
    auto unused = ::write(m_pipe.second, &ptrval, sizeof(ptrval));
    static_cast<void>(unused);
# endif
}

default_multiplexer::runnable* default_multiplexer::rd_dispatch_request() {
  intptr_t ptrval;
  // on windows, we actually have sockets, otherwise we have file handles
# ifdef CAF_WINDOWS
    ::recv(m_pipe.first, reinterpret_cast<socket_recv_ptr>(&ptrval),
           sizeof(ptrval), 0);
# else
    auto unused = ::read(m_pipe.first, &ptrval, sizeof(ptrval));
    static_cast<void>(unused);
# endif
  return reinterpret_cast<runnable*>(ptrval);;
}

default_multiplexer& get_multiplexer_singleton() {
  return static_cast<default_multiplexer&>(middleman::instance()->backend());
}

multiplexer::supervisor_ptr default_multiplexer::make_supervisor() {
  class impl : public multiplexer::supervisor {
   public:
    impl(default_multiplexer* thisptr) : m_this(thisptr) {
      // nop
    }
    ~impl() {
      auto ptr = m_this;
      ptr->dispatch([=] { ptr->close_pipe(); });
    }
   private:
    default_multiplexer* m_this;
  };
  return supervisor_ptr{new impl(this)};
}

void default_multiplexer::close_pipe() {
  CAF_LOG_TRACE("");
  del(operation::read, m_pipe.first, nullptr);
}

void default_multiplexer::handle_socket_event(native_socket fd, int mask,
                                      event_handler* ptr) {
  CAF_LOG_TRACE(CAF_ARG(fd) << ", " << CAF_ARG(mask));
  bool checkerror = true;
  if (mask & input_mask) {
    checkerror = false;
    if (ptr) {
      ptr->handle_event(operation::read);
    } else {
      CAF_REQUIRE(fd == m_pipe.first);
      CAF_LOG_DEBUG("read message from pipe");
      auto cb = rd_dispatch_request();
      cb->run();
      cb->request_deletion();
    }
  }
  if (mask & output_mask) {
    // we do *never* register our pipe handle for writing
    CAF_REQUIRE(ptr != nullptr);
    checkerror = false;
    ptr->handle_event(operation::write);
  }
  if (checkerror && (mask & error_mask)) {
    if (ptr) {
      CAF_LOG_DEBUG("error occured on socket "
                    << fd << ", error code: "
                    << last_socket_error()
                    << ", error string: "
                    << last_socket_error_as_string());
      ptr->handle_event(operation::propagate_error);
      del(operation::read, fd, ptr);
      del(operation::write, fd, ptr);
    } else {
      CAF_LOG_DEBUG("pipe has been closed, assume shutdown");
      // a pipe failure means we are in the process
      // of shutting down the middleman
      del(operation::read, fd, nullptr);
    }
  }
  CAF_LOG_DEBUG_IF(!checkerror && (mask & error_mask),
                   "ignored error because epoll still reported read or write "
                   "event; wait until no other event occurs before "
                   "handling error");
}

void default_multiplexer::init() {
# ifdef CAF_WINDOWS
    WSADATA WinsockData;
    if (WSAStartup(MAKEWORD(2, 2), &WinsockData) != 0) {
        CAF_CRITICAL("WSAStartup failed");
    }
# endif
}

default_multiplexer::~default_multiplexer() {
# ifdef CAF_WINDOWS
    WSACleanup();
# endif
  if (m_epollfd != invalid_native_socket) {
    closesocket(m_epollfd);
  }
  closesocket(m_pipe.first);
  closesocket(m_pipe.second);
}

void default_multiplexer::dispatch_runnable(runnable_ptr ptr) {
  wr_dispatch_request(ptr.release());
}

connection_handle default_multiplexer::add_tcp_scribe(broker* self,
                                                       default_socket&& sock) {
  CAF_LOG_TRACE("");
  class impl : public broker::scribe {
   public:
    impl(broker* parent, default_socket&& s)
        : scribe(parent, network::conn_hdl_from_socket(s)),
          m_launched(false),
          m_stream(s.backend()) {
      m_stream.init(std::move(s));
    }
    void configure_read(receive_policy::config config) override {
      CAF_LOGM_TRACE("caf::io::broker::scribe", "");
      m_stream.configure_read(config);
      if (!m_launched) launch();
    }
    broker::buffer_type& wr_buf() override {
      return m_stream.wr_buf();
    }
    broker::buffer_type& rd_buf() override {
      return m_stream.rd_buf();
    }
    void stop_reading() override {
      CAF_LOGM_TRACE("caf::io::broker::scribe", "");
      m_stream.stop_reading();
      disconnect(false);
    }
    void flush() override {
      CAF_LOGM_TRACE("caf::io::broker::scribe", "");
      m_stream.flush(this);
    }
    void launch() {
      CAF_LOGM_TRACE("caf::io::broker::scribe", "");
      CAF_REQUIRE(!m_launched);
      m_launched = true;
      m_stream.start(this);
    }
   private:
    bool m_launched;
    stream<default_socket> m_stream;
  };
  broker::scribe_pointer ptr{new impl{self, std::move(sock)}};
  self->add_scribe(ptr);
  return ptr->hdl();
}

accept_handle default_multiplexer::add_tcp_doorman(broker* self,
                                                   default_socket_acceptor&& sock) {
  CAF_LOG_TRACE("sock.fd = " << sock.fd());
  CAF_REQUIRE(sock.fd() != network::invalid_native_socket);
  class impl : public broker::doorman {
   public:
    impl(broker* parent, default_socket_acceptor&& s)
        : doorman(parent, network::accept_hdl_from_socket(s)),
          m_acceptor(s.backend()) {
      m_acceptor.init(std::move(s));
    }
    void new_connection() override {
      auto& dm = m_acceptor.backend();
      accept_msg().handle = dm.add_tcp_scribe(parent(),
                                              std::move(m_acceptor.accepted_socket()));
      parent()->invoke_message(invalid_actor_addr,
                               invalid_message_id,
                               m_accept_msg);
    }
    void stop_reading() override {
      m_acceptor.stop_reading();
      disconnect(false);
    }
    void launch() override {
      m_acceptor.start(this);
    }
   private:
    network::acceptor<default_socket_acceptor> m_acceptor;
  };
  broker::doorman_pointer ptr{new impl{self, std::move(sock)}};
  self->add_doorman(ptr);
  return ptr->hdl();
}

connection_handle default_multiplexer::add_tcp_scribe(broker* self,
                                                      native_socket fd) {
  return add_tcp_scribe(self, default_socket{*this, fd});
}

connection_handle default_multiplexer::add_tcp_scribe(broker* self,
                                                      const std::string& host,
                                                      uint16_t port) {
  return add_tcp_scribe(self, new_ipv4_connection(host, port));
}

accept_handle default_multiplexer::add_tcp_doorman(broker* self,
                                                   native_socket fd) {
  return add_tcp_doorman(self, default_socket_acceptor{*this, fd});
}

accept_handle default_multiplexer::add_tcp_doorman(broker* self,
                                                   uint16_t port,
                                                   const char* host) {
  return add_tcp_doorman(self, new_ipv4_acceptor(port, host));
}

/******************************************************************************
 *               platform-independent implementations (finally)               *
 ******************************************************************************/

void throw_io_failure(const char* what, bool add_errno) {
  if (add_errno) {
    std::ostringstream oss;
    oss << what << ": " << last_socket_error_as_string()
        << " [errno: " << last_socket_error() << "]";
    throw network_error(oss.str());
  }
  throw network_error(what);
}

void tcp_nodelay(native_socket fd, bool new_value) {
  int flag = new_value ? 1 : 0;
  if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
                 reinterpret_cast<setsockopt_ptr>(&flag), sizeof(flag)) < 0) {
    throw_io_failure("unable to set TCP_NODELAY");
  }
}

bool is_error(ssize_t res, bool is_nonblock) {
  if (res < 0) {
    auto err = last_socket_error();
    if (!is_nonblock || !would_block_or_temporarily_unavailable(err)) {
      return true;
    }
    // don't report an error in case of
    // spurious wakeup or something similar
  }
  return false;
}

bool read_some(size_t& result, native_socket fd, void* buf, size_t len) {
  CAF_LOGF_TRACE(CAF_ARG(fd) << ", " << CAF_ARG(len));
  auto sres = ::recv(fd, reinterpret_cast<socket_recv_ptr>(buf), len, 0);
  CAF_LOGF_DEBUG("tried to read " << len << " bytes from socket " << fd
                                  << ", recv returned " << sres);
  if (is_error(sres, true) || sres == 0) {
    // recv returns 0  when the peer has performed an orderly shutdown
    return false;
  }
  result = (sres > 0) ? static_cast<size_t>(sres) : 0;
  return true;
}

bool write_some(size_t& result, native_socket fd, const void* buf, size_t len) {
  CAF_LOGF_TRACE(CAF_ARG(fd) << ", " << CAF_ARG(len));
  auto sres = ::send(fd, reinterpret_cast<socket_send_ptr>(buf), len, 0);
  CAF_LOGF_DEBUG("tried to write " << len << " bytes to socket " << fd
                                   << ", send returned " << sres);
  if (is_error(sres, true))
    return false;
  result = (sres > 0) ? static_cast<size_t>(sres) : 0;
  return true;
}

bool try_accept(native_socket& result, native_socket fd) {
  CAF_LOGF_TRACE(CAF_ARG(fd));
  sockaddr addr;
  memset(&addr, 0, sizeof(addr));
  socklen_t addrlen = sizeof(addr);
  result = ::accept(fd, &addr, &addrlen);
  CAF_LOGF_DEBUG("tried to accept a new connection from from socket "
                 << fd << ", accept returned " << result);
  if (result == invalid_native_socket) {
    auto err = last_socket_error();
    if (!would_block_or_temporarily_unavailable(err)) {
      return false;
    }
  }
  return true;
}

event_handler::event_handler(default_multiplexer& dm)
    : m_backend(dm),
      m_eventbf(0) {
  // nop
}

event_handler::~event_handler() {
  // nop
}

default_socket::default_socket(default_multiplexer& parent, native_socket fd)
    : m_parent(parent),
      m_fd(fd) {
  CAF_LOG_TRACE(CAF_ARG(fd));
  if (fd != invalid_native_socket) {
    // enable nonblocking IO & disable Nagle's algorithm
    nonblocking(m_fd, true);
    tcp_nodelay(m_fd, true);
  }
}

default_socket::default_socket(default_socket&& other)
    : m_parent(other.m_parent), m_fd(other.m_fd) {
  other.m_fd = invalid_native_socket;
}

default_socket& default_socket::operator=(default_socket&& other) {
  std::swap(m_fd, other.m_fd);
  return *this;
}

default_socket::~default_socket() {
  if (m_fd != invalid_native_socket) {
    CAF_LOG_DEBUG("close socket " << m_fd);
    closesocket(m_fd);
  }
}

void default_socket::close_read() {
  if (m_fd != invalid_native_socket) {
    ::shutdown(m_fd, 0); // 0 identifyies the read channel on Win & UNIX
  }
}

struct socket_guard {
 public:
  socket_guard(native_socket fd) : m_fd(fd) {
    // nop
  }
  ~socket_guard() {
    if (m_fd != invalid_native_socket)
      closesocket(m_fd);
  }
  void release() {
    m_fd = invalid_native_socket;
  }
 private:
  native_socket m_fd;
};

native_socket new_ipv4_connection_impl(const std::string& host, uint16_t port) {
  CAF_LOGF_TRACE(CAF_ARG(host) << ", " << CAF_ARG(port));
  CAF_LOGF_INFO("try to connect to " << host << " on port " << port);
# ifdef CAF_WINDOWS
    // make sure TCP has been initialized via WSAStartup
    get_multiplexer_singleton();
# endif
  sockaddr_in serv_addr;
  hostent* server;
  native_socket fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd == invalid_native_socket) {
    throw network_error("socket creation failed");
  }
  socket_guard sguard(fd);
  server = gethostbyname(host.c_str());
  if (!server) {
    std::string errstr = "no such host: ";
    errstr += host;
    throw network_error(std::move(errstr));
  }
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  memmove(&serv_addr.sin_addr.s_addr, server->h_addr,
          static_cast<size_t>(server->h_length));
  serv_addr.sin_port = htons(port);
  CAF_LOGF_DEBUG("call connect()");
  if (connect(fd, (const sockaddr*)&serv_addr, sizeof(serv_addr)) != 0) {
    CAF_LOGF_ERROR("could not connect to to " << host << " on port " << port);
    throw network_error("could not connect to host");
  }
  sguard.release();
  return fd;
}

default_socket new_ipv4_connection(const std::string& host, uint16_t port) {
  auto& backend = get_multiplexer_singleton();
  return default_socket{backend, new_ipv4_connection_impl(host, port)};
}

native_socket new_ipv4_acceptor_impl(uint16_t port, const char* addr) {
  CAF_LOGF_TRACE(CAF_ARG(port)
          << ", addr = " << (addr ? addr : "nullptr"));
# ifdef CAF_WINDOWS
    // make sure TCP has been initialized via WSAStartup
    get_multiplexer_singleton();
# endif
  native_socket fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd == invalid_native_socket) {
    throw network_error("could not create server socket");
  }
  // sguard closes the socket in case of exception
  socket_guard sguard(fd);
  int on = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<setsockopt_ptr>(&on), sizeof(on)) < 0) {
    throw_io_failure("unable to set SO_REUSEADDR");
  }
  struct sockaddr_in serv_addr;
  memset((char*)&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  if (!addr) {
    serv_addr.sin_addr.s_addr = INADDR_ANY;
  } else if (::inet_pton(AF_INET, addr, &serv_addr.sin_addr) <= 0) {
    throw network_error("invalid IPv4 address");
  }
  serv_addr.sin_port = htons(port);
  if (bind(fd, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    throw bind_failure(last_socket_error_as_string());
  }
  if (listen(fd, SOMAXCONN) != 0) {
    throw network_error("listen() failed");
  }
  // ok, no exceptions so far
  sguard.release();
  CAF_LOGF_DEBUG("sockfd = " << fd);
  return fd;
}

default_socket_acceptor new_ipv4_acceptor(uint16_t port, const char* addr) {
  CAF_LOGF_TRACE(CAF_ARG(port) << ", addr = " << (addr ? addr : "nullptr"));
  auto& backend = get_multiplexer_singleton();
  return default_socket_acceptor{backend, new_ipv4_acceptor_impl(port, addr)};
}

} // namespace network
} // namespace io
} // namespace caf
