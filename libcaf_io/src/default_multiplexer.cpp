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

#include "caf/io/network/default_multiplexer.hpp"

#include "caf/config.hpp"
#include "caf/optional.hpp"
#include "caf/make_counted.hpp"
#include "caf/actor_system_config.hpp"

#include "caf/scheduler/abstract_coordinator.hpp"

#include "caf/io/broker.hpp"
#include "caf/io/middleman.hpp"

#include "caf/io/network/protocol.hpp"
#include "caf/io/network/interfaces.hpp"

#ifdef CAF_WINDOWS
# include <winsock2.h>
# include <ws2tcpip.h> // socklen_t, etc. (MSVC20xx)
# include <windows.h>
# include <io.h>
#else
# include <cerrno>
# include <netdb.h>
# include <fcntl.h>
# include <sys/types.h>
# include <arpa/inet.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/tcp.h>

#include <utility>
#endif

using std::string;

// -- Utiliy functions for converting errno into CAF errors --------------------

namespace {

#if defined(CAF_MACOS) || defined(CAF_IOS)
  constexpr int no_sigpipe_flag = SO_NOSIGPIPE;
#elif defined(CAF_WINDOWS)
  constexpr int no_sigpipe_flag = 0; // does not exist on Windows
#else // BSD, Linux or Android
  constexpr int no_sigpipe_flag = MSG_NOSIGNAL;
#endif

// safe ourselves some typing
constexpr auto ipv4 = caf::io::network::protocol::ipv4;
constexpr auto ipv6 = caf::io::network::protocol::ipv6;

// predicate for `ccall` meaning "expected result of f is 0"
bool cc_zero(int value) {
  return value == 0;
}

// predicate for `ccall` meaning "expected result of f is 1"
bool cc_one(int value) {
  return value == 1;
}

// predicate for `ccall` meaning "expected result of f is not -1"
bool cc_not_minus1(int value) {
  return value != -1;
}

// predicate for `ccall` meaning "expected result of f is a valid socket"
bool cc_valid_socket(caf::io::network::native_socket fd) {
  return fd != caf::io::network::invalid_native_socket;
}

// calls a C functions and returns an error if `predicate(var)`  returns false
#define CALL_CFUN(var, predicate, fun_name, expr)                              \
  auto var = expr;                                                             \
  if (!predicate(var))                                                         \
    return make_error(sec::network_syscall_failed,                             \
                      fun_name, last_socket_error_as_string())

// calls a C functions and calls exit() if `predicate(var)`  returns false
#ifdef CAF_WINDOWS
#define CALL_CRITICAL_CFUN(var, predicate, funname, expr)                      \
  auto var = expr;                                                             \
  if (!predicate(var)) {                                                       \
    fprintf(stderr, "[FATAL] %s:%u: syscall failed: %s returned %s\n",         \
           __FILE__, __LINE__, funname, last_socket_error_as_string().c_str());\
    abort();                                                                   \
  } static_cast<void>(0)
#endif // CAF_WINDOWS

} // namespace <anonymous>

namespace caf {
namespace io {
namespace network {

// -- OS-specific functions for sockets and pipes ------------------------------

#ifndef CAF_WINDOWS

  string last_socket_error_as_string() {
    return strerror(errno);
  }

  expected<void> nonblocking(native_socket fd, bool new_value) {
    CAF_LOG_TRACE(CAF_ARG(fd) << CAF_ARG(new_value));
    // read flags for fd
    CALL_CFUN(rf, cc_not_minus1, "fcntl", fcntl(fd, F_GETFL, 0));
    // calculate and set new flags
    auto wf = new_value ? (rf | O_NONBLOCK) : (rf & (~(O_NONBLOCK)));
    CALL_CFUN(set_res, cc_not_minus1, "fcntl", fcntl(fd, F_SETFL, wf));
    return unit;
  }

  expected<void> allow_sigpipe(native_socket fd, bool new_value) {
#   if !defined(CAF_LINUX) && !defined(CAF_CYGWIN)
    int value = new_value ? 0 : 1;
    CALL_CFUN(res, cc_zero, "setsockopt",
              setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &value,
                         static_cast<unsigned>(sizeof(value))));
#   else
    // SO_NOSIGPIPE does not exist on Linux, suppress unused warnings
    static_cast<void>(fd);
    static_cast<void>(new_value);
#   endif
    return unit;
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

  expected<void> nonblocking(native_socket fd, bool new_value) {
    u_long mode = new_value ? 1 : 0;
    CALL_CFUN(res, cc_zero, "ioctlsocket", ioctlsocket(fd, FIONBIO, &mode));
    return unit;
  }

  expected<void> allow_sigpipe(native_socket, bool) {
    // nop; SIGPIPE does not exist on Windows
    return unit;
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
    CALL_CRITICAL_CFUN(listener, cc_valid_socket, "socket",
                       socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    union {
      sockaddr_in inaddr;
      sockaddr addr;
    } a;
    memset(&a, 0, sizeof(a));
    a.inaddr.sin_family = AF_INET;
    a.inaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.inaddr.sin_port = 0;
    // makes sure all sockets are closed in case of an error
    auto guard = detail::make_scope_guard([&] {
      auto e = WSAGetLastError();
      closesocket(listener);
      closesocket(socks[0]);
      closesocket(socks[1]);
      WSASetLastError(e);
    });
    // bind listener to a local port
    int reuse = 1;
    CALL_CRITICAL_CFUN(tmp1, cc_zero, "setsockopt",
                       setsockopt(listener, SOL_SOCKET, SO_REUSEADDR,
                                  reinterpret_cast<char*>(&reuse),
                                  static_cast<int>(sizeof(reuse))));
    CALL_CRITICAL_CFUN(tmp2, cc_zero, "bind",
                       bind(listener, &a.addr,
                            static_cast<int>(sizeof(a.inaddr))));
    // read the port in use: win32 getsockname may only set the port number
    // (http://msdn.microsoft.com/library/ms738543.aspx):
    memset(&a, 0, sizeof(a));
    CALL_CRITICAL_CFUN(tmp3, cc_zero, "getsockname",
                       getsockname(listener, &a.addr, &addrlen));
    a.inaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.inaddr.sin_family = AF_INET;
    // set listener to listen mode
    CALL_CRITICAL_CFUN(tmp5, cc_zero, "listen", listen(listener, 1));
    // create read-only end of the pipe
    DWORD flags = 0;
    CALL_CRITICAL_CFUN(read_fd, cc_valid_socket, "WSASocketW",
                       WSASocketW(AF_INET, SOCK_STREAM, 0, nullptr, 0, flags));
    CALL_CRITICAL_CFUN(tmp6, cc_zero, "connect",
                       connect(read_fd, &a.addr,
                               static_cast<int>(sizeof(a.inaddr))));
    // get write-only end of the pipe
    CALL_CRITICAL_CFUN(write_fd, cc_valid_socket, "accept",
                       accept(listener, nullptr, nullptr));
    closesocket(listener);
    guard.disable();
    return std::make_pair(read_fd, write_fd);
  }

#endif

// -- Platform-dependent abstraction over epoll() or poll() --------------------

#ifdef CAF_EPOLL_MULTIPLEXER

  // In this implementation, shadow_ is the number of sockets we have
  // registered to epoll.

  default_multiplexer::default_multiplexer(actor_system* sys)
      : multiplexer(sys),
        epollfd_(invalid_native_socket),
        shadow_(1),
        pipe_reader_(*this) {
    init();
    epollfd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epollfd_ == -1) {
      CAF_LOG_ERROR("epoll_create1: " << strerror(errno));
      exit(errno);
    }
    // handle at most 64 events at a time
    pollset_.resize(64);
    pipe_ = create_pipe();
    pipe_reader_.init(pipe_.first);
    epoll_event ee;
    ee.events = input_mask;
    ee.data.ptr = &pipe_reader_;
    if (epoll_ctl(epollfd_, EPOLL_CTL_ADD, pipe_reader_.fd(), &ee) < 0) {
      CAF_LOG_ERROR("epoll_ctl: " << strerror(errno));
      exit(errno);
    }
  }

  bool default_multiplexer::poll_once(bool block) {
    CAF_LOG_TRACE("epoll()-based multiplexer");
    // Keep running in case of `EINTR`.
    for (;;) {
      int presult = epoll_wait(epollfd_, pollset_.data(),
                               static_cast<int>(pollset_.size()),
                               block ? -1 : 0);
      CAF_LOG_DEBUG("epoll_wait() on"      << shadow_
                    << "sockets reported" << presult << "event(s)");
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
      if (presult == 0)
        return false;
      auto iter = pollset_.begin();
      auto last = iter + presult;
      for (; iter != last; ++iter) {
        auto ptr = reinterpret_cast<event_handler*>(iter->data.ptr);
        auto fd = ptr ? ptr->fd() : pipe_.first;
        handle_socket_event(fd, static_cast<int>(iter->events), ptr);
      }
      for (auto& me : events_) {
        handle(me);
      }
      events_.clear();
      return true;
    }
  }

  void default_multiplexer::run() {
    CAF_LOG_TRACE("epoll()-based multiplexer");
    while (shadow_ > 0)
      poll_once(true);
  }

  void default_multiplexer::handle(const default_multiplexer::event& e) {
    CAF_LOG_TRACE("e.fd = " << CAF_ARG(e.fd) << ", mask = "
                  << CAF_ARG(e.mask));
    // ptr is only allowed to nullptr if fd is our pipe
    // read handle which is only registered for input
    CAF_ASSERT(e.ptr != nullptr || e.fd == pipe_.first);
    if (e.ptr && e.ptr->eventbf() == e.mask) {
      // nop
      return;
    }
    auto old = e.ptr ? e.ptr->eventbf() : input_mask;
    if (e.ptr){
      e.ptr->eventbf(e.mask);
    }
    epoll_event ee;
    ee.events = static_cast<uint32_t>(e.mask);
    ee.data.ptr = e.ptr;
    int op;
    if (e.mask == 0) {
      CAF_LOG_DEBUG("attempt to remove socket " << CAF_ARG(e.fd)
                    << " from epoll");
      op = EPOLL_CTL_DEL;
      --shadow_;
    } else if (old == 0) {
      CAF_LOG_DEBUG("attempt to add socket " << CAF_ARG(e.fd) << " to epoll");
      op = EPOLL_CTL_ADD;
      ++shadow_;
    } else {
      CAF_LOG_DEBUG("modify epoll event mask for socket " << CAF_ARG(e.fd)
                    << ": " << CAF_ARG(old) << " -> " << CAF_ARG(e.mask));
      op = EPOLL_CTL_MOD;
    }
    if (epoll_ctl(epollfd_, op, e.fd, &ee) < 0) {
      switch (last_socket_error()) {
        // supplied file descriptor is already registered
        case EEXIST:
          CAF_LOG_ERROR("file descriptor registered twice");
          --shadow_;
          break;
        // op was EPOLL_CTL_MOD or EPOLL_CTL_DEL,
        // and fd is not registered with this epoll instance.
        case ENOENT:
          CAF_LOG_ERROR(
            "cannot delete file descriptor "
            "because it isn't registered");
          if (e.mask == 0) {
            ++shadow_;
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
  // vectors in this implementation: pollset_ and shadow_. The former
  // stores our pollset, the latter stores our pointers. Both vectors
  // are sorted by the file descriptor. This allows us to quickly,
  // i.e., O(1), access the actual object when handling socket events.

  default_multiplexer::default_multiplexer(actor_system* sys)
      : multiplexer(sys),
        epollfd_(-1),
        pipe_reader_(*this) {
    init();
    // initial setup
    pipe_ = create_pipe();
    pipe_reader_.init(pipe_.first);
    pollfd pipefd;
    pipefd.fd = pipe_reader_.fd();
    pipefd.events = input_mask;
    pipefd.revents = 0;
    pollset_.push_back(pipefd);
    shadow_.push_back(&pipe_reader_);
  }

  bool default_multiplexer::poll_once(bool block) {
    CAF_LOG_TRACE("poll()-based multiplexer");
    // we store the results of poll() in a separate vector , because
    // altering the pollset while traversing it is not exactly a
    // bright idea ...
    struct fd_event {
      native_socket  fd;      // our file descriptor
      short          mask;    // the event mask returned by poll()
      event_handler* ptr;     // nullptr in case of a pipe event
    };
    std::vector<fd_event> poll_res;
    for(;;) {
      int presult;
#     ifdef CAF_WINDOWS
        presult = ::WSAPoll(pollset_.data(),
                            static_cast<ULONG>(pollset_.size()),
                            block ? -1 : 0);
#     else
        presult = ::poll(pollset_.data(),
                         static_cast<nfds_t>(pollset_.size()), block ? -1 : 0);
#     endif
      CAF_LOG_DEBUG("poll() on" << pollset_.size() 
                    << "sockets reported" << presult << "event(s)");
      if (presult < 0) {
        switch (last_socket_error()) {
          case EINTR: {
            CAF_LOG_DEBUG("received EINTR, try again");
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
      if (presult == 0)
        return false;
      // scan pollset for events first, because we might alter pollset_
      // while running callbacks (not a good idea while traversing it)
      CAF_LOG_DEBUG("scan pollset for socket events");
      for (size_t i = 0; i < pollset_.size() && presult > 0; ++i) {
        auto& pfd = pollset_[i];
        if (pfd.revents != 0) {
          CAF_LOG_DEBUG("event on socket:" << CAF_ARG(pfd.fd)
                        << CAF_ARG(pfd.revents));
          poll_res.push_back({pfd.fd, pfd.revents, shadow_[i]});
          pfd.revents = 0;
          --presult; // stop as early as possible
        }
      }
      CAF_LOG_DEBUG(CAF_ARG(poll_res.size()));
      for (auto& e : poll_res) {
        // we try to read/write as much as possible by ignoring
        // error states as long as there are still valid
        // operations possible on the socket
        handle_socket_event(e.fd, e.mask, e.ptr);
      }
      CAF_LOG_DEBUG(CAF_ARG(events_.size()));
      poll_res.clear();
      for (auto& me : events_) {
        handle(me);
      }
      events_.clear();
      return true;
    }
  }

  void default_multiplexer::run() {
    CAF_LOG_TRACE("poll()-based multiplexer:" << CAF_ARG(input_mask)
                  << CAF_ARG(output_mask) << CAF_ARG(error_mask));
    while (!pollset_.empty())
      poll_once(true);
  }

  void default_multiplexer::handle(const default_multiplexer::event& e) {
    CAF_ASSERT(e.fd != invalid_native_socket);
    CAF_ASSERT(pollset_.size() == shadow_.size());
    CAF_LOG_TRACE(CAF_ARG(e.fd) << CAF_ARG(e.mask));
    auto last = pollset_.end();
    auto i = std::lower_bound(pollset_.begin(), last, e.fd,
                              [](const pollfd& lhs, native_socket rhs) {
                                return lhs.fd < rhs;
                              });
    pollfd new_element;
    new_element.fd = e.fd;
    new_element.events = static_cast<short>(e.mask);
    new_element.revents = 0;
    int old_mask = 0;
    if (e.ptr != nullptr) {
      old_mask = e.ptr->eventbf();
      e.ptr->eventbf(e.mask);
    }
    // calculate shadow of i
    multiplexer_poll_shadow_data::iterator j;
    if (i == last) {
      j = shadow_.end();
    } else {
      j = shadow_.begin();
      std::advance(j, distance(pollset_.begin(), i));
    }
    // modify vectors
    if (i == last) { // append
      if (e.mask != 0) {
        pollset_.push_back(new_element);
        shadow_.push_back(e.ptr);
      }
    } else if (i->fd == e.fd) { // modify
      if (e.mask == 0) {
        // delete item
        pollset_.erase(i);
        shadow_.erase(j);
      } else {
        // update event mask of existing entry
        CAF_ASSERT(*j == e.ptr);
        i->events = static_cast<short>(e.mask);
      }
      if (e.ptr != nullptr) {
        auto remove_from_loop_if_needed = [&](int flag, operation flag_op) {
          if (((old_mask & flag) != 0) && ((e.mask & flag) == 0)) {
            e.ptr->removed_from_loop(flag_op);
          }
        };
        remove_from_loop_if_needed(input_mask, operation::read);
        remove_from_loop_if_needed(output_mask, operation::write);
      }
    } else { // insert at iterator pos
      pollset_.insert(i, new_element);
      shadow_.insert(j, e.ptr);
    }
  }

#endif // CAF_EPOLL_MULTIPLEXER

// -- Helper functions for defining bitmasks of event handlers -----------------

int add_flag(operation op, int bf) {
  switch (op) {
    case operation::read:
      return bf | input_mask;
    case operation::write:
      return bf | output_mask;
    case operation::propagate_error:
      CAF_LOG_ERROR("unexpected operation");
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
    case operation::propagate_error:
      CAF_LOG_ERROR("unexpected operation");
      break;
  }
  // weird stuff going on
  return 0;
}

// -- Platform-independent free functions --------------------------------------

expected<void> tcp_nodelay(native_socket fd, bool new_value) {
  CAF_LOG_TRACE(CAF_ARG(fd) << CAF_ARG(new_value));
  int flag = new_value ? 1 : 0;
  CALL_CFUN(res, cc_zero, "setsockopt",
            setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
                       reinterpret_cast<setsockopt_ptr>(&flag),
                       static_cast<socklen_t>(sizeof(flag))));
  return unit;
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

rw_state read_some(size_t& result, native_socket fd, void* buf, size_t len) {
  CAF_LOG_TRACE(CAF_ARG(fd) << CAF_ARG(len));
  auto sres = ::recv(fd, reinterpret_cast<socket_recv_ptr>(buf), len, 0);
  CAF_LOG_DEBUG(CAF_ARG(len) << CAF_ARG(fd) << CAF_ARG(sres));
  if (is_error(sres, true) || sres == 0) {
    // recv returns 0  when the peer has performed an orderly shutdown
    return rw_state::failure;
  }
  result = (sres > 0) ? static_cast<size_t>(sres) : 0;
  return rw_state::success;
}

rw_state write_some(size_t& result, native_socket fd, const void* buf,
                    size_t len) {
  CAF_LOG_TRACE(CAF_ARG(fd) << CAF_ARG(len));
  auto sres = ::send(fd, reinterpret_cast<socket_send_ptr>(buf),
                     len, no_sigpipe_flag);
  CAF_LOG_DEBUG(CAF_ARG(len) << CAF_ARG(fd) << CAF_ARG(sres));
  if (is_error(sres, true))
    return rw_state::failure;
  result = (sres > 0) ? static_cast<size_t>(sres) : 0;
  return rw_state::success;
}

 bool try_accept(native_socket& result, native_socket fd) {
  CAF_LOG_TRACE(CAF_ARG(fd));
  sockaddr_storage addr;
  memset(&addr, 0, sizeof(addr));
  socklen_t addrlen = sizeof(addr);
  result = ::accept(fd, reinterpret_cast<sockaddr*>(&addr), &addrlen);
  CAF_LOG_DEBUG(CAF_ARG(fd) << CAF_ARG(result));
  if (result == invalid_native_socket) {
    auto err = last_socket_error();
    if (!would_block_or_temporarily_unavailable(err)) {
      return false;
    }
  }
  return true;
}

// -- Policy class for TCP wrapping above free functions -----------------------

read_some_fun tcp_policy::read_some = network::read_some;

write_some_fun tcp_policy::write_some = network::write_some;

try_accept_fun tcp_policy::try_accept = network::try_accept;

// -- Platform-independent parts of the default_multiplexer --------------------

bool default_multiplexer::try_run_once() {
  return poll_once(false);
}

void default_multiplexer::run_once() {
  poll_once(true);
}

void default_multiplexer::add(operation op, native_socket fd,
                              event_handler* ptr) {
  CAF_ASSERT(fd != invalid_native_socket);
  // ptr == nullptr is only allowed to store our pipe read handle
  // and the pipe read handle is added in the ctor (not allowed here)
  CAF_ASSERT(ptr != nullptr);
  CAF_LOG_TRACE(CAF_ARG(op) << CAF_ARG(fd));
  new_event(add_flag, op, fd, ptr);
}

void default_multiplexer::del(operation op, native_socket fd,
                              event_handler* ptr) {
  CAF_ASSERT(fd != invalid_native_socket);
  // ptr == nullptr is only allowed when removing our pipe read handle
  CAF_ASSERT(ptr != nullptr || fd == pipe_.first);
  CAF_LOG_TRACE(CAF_ARG(op)<< CAF_ARG(fd));
  new_event(del_flag, op, fd, ptr);
}

void default_multiplexer::wr_dispatch_request(resumable* ptr) {
  intptr_t ptrval = reinterpret_cast<intptr_t>(ptr);
  // on windows, we actually have sockets, otherwise we have file handles
# ifdef CAF_WINDOWS
  auto res = ::send(pipe_.second, reinterpret_cast<socket_send_ptr>(&ptrval),
                    sizeof(ptrval), no_sigpipe_flag);
# else
  auto res = ::write(pipe_.second, &ptrval, sizeof(ptrval));
# endif
  if (res <= 0) {
    // pipe closed, discard resumable
    intrusive_ptr_release(ptr);
  } else if (static_cast<size_t>(res) < sizeof(ptrval)) {
    // must not happen: wrote invalid pointer to pipe
    std::cerr << "[CAF] Fatal error: wrote invalid data to pipe" << std::endl;
    abort();
  }
}

multiplexer::supervisor_ptr default_multiplexer::make_supervisor() {
  class impl : public multiplexer::supervisor {
  public:
    explicit impl(default_multiplexer* thisptr) : this_(thisptr) {
      // nop
    }
    ~impl() override {
      auto ptr = this_;
      ptr->dispatch([=] { ptr->close_pipe(); });
    }
  private:
    default_multiplexer* this_;
  };
  return supervisor_ptr{new impl(this)};
}

void default_multiplexer::close_pipe() {
  CAF_LOG_TRACE("");
  del(operation::read, pipe_.first, nullptr);
}

void default_multiplexer::handle_socket_event(native_socket fd, int mask,
                                              event_handler* ptr) {
  CAF_LOG_TRACE(CAF_ARG(fd) << CAF_ARG(mask));
  CAF_ASSERT(ptr != nullptr);
  bool checkerror = true;
  if ((mask & input_mask) != 0) {
    checkerror = false;
    // ignore read events if a previous event caused
    // this socket to be shut down for reading
    if (!ptr->read_channel_closed())
      ptr->handle_event(operation::read);
  }
  if ((mask & output_mask) != 0) {
    checkerror = false;
    ptr->handle_event(operation::write);
  }
  if (checkerror && ((mask & error_mask) != 0)) {
    CAF_LOG_DEBUG("error occured on socket:"
                  << CAF_ARG(fd) << CAF_ARG(last_socket_error())
                  << CAF_ARG(last_socket_error_as_string()));
    ptr->handle_event(operation::propagate_error);
    del(operation::read, fd, ptr);
    del(operation::write, fd, ptr);
  }
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
  if (epollfd_ != invalid_native_socket)
    closesocket(epollfd_);
  // close write handle first
  closesocket(pipe_.second);
  // flush pipe before closing it
  nonblocking(pipe_.first, true);
  auto ptr = pipe_reader_.try_read_next();
  while (ptr != nullptr) {
    scheduler::abstract_coordinator::cleanup_and_release(ptr);
    ptr = pipe_reader_.try_read_next();
  }
  // do cleanup for pipe reader manually, since WSACleanup needs to happen last
  closesocket(pipe_reader_.fd());
  pipe_reader_.init(invalid_native_socket);
# ifdef CAF_WINDOWS
  WSACleanup();
# endif
}

void default_multiplexer::exec_later(resumable* ptr, bool) {
  CAF_ASSERT(ptr);
  switch (ptr->subtype()) {
    case resumable::io_actor:
    case resumable::function_object:
      wr_dispatch_request(ptr);
      break;
    default:
     system().scheduler().enqueue(ptr);
  }
}

bool default_multiplexer::is_neighbor(execution_unit*) const {
  return false;
}

scribe_ptr default_multiplexer::new_scribe(native_socket fd) {
  CAF_LOG_TRACE("");
  return make_counted<scribe_impl>(*this, fd);
}

expected<scribe_ptr>
default_multiplexer::new_tcp_scribe(const std::string& host, uint16_t port) {
  auto fd = new_tcp_connection(host, port);
  if (!fd)
    return std::move(fd.error());
  return new_scribe(*fd);
}

doorman_ptr default_multiplexer::new_doorman(native_socket fd) {
  CAF_LOG_TRACE(CAF_ARG(fd));
  CAF_ASSERT(fd != network::invalid_native_socket);
  return make_counted<doorman_impl>(*this, fd);
}

expected<doorman_ptr> default_multiplexer::new_tcp_doorman(uint16_t port,
                                                           const char* in,
                                                           bool reuse_addr) {
  auto fd = new_tcp_acceptor_impl(port, in, reuse_addr);
  if (fd)
    return new_doorman(*fd);
  return std::move(fd.error());
}


event_handler::event_handler(default_multiplexer& dm, native_socket sockfd)
    : eventbf_(0),
      fd_(sockfd),
      read_channel_closed_(false),
      backend_(dm) {
  set_fd_flags();
}

event_handler::~event_handler() {
  if (fd_ != invalid_native_socket)
    closesocket(fd_);
}

void event_handler::close_read_channel() {
  if (fd_ == invalid_native_socket || read_channel_closed_)
    return;
  ::shutdown(fd_, 0); // 0 identifies the read channel on Win & UNIX
  read_channel_closed_ = true;
}

void event_handler::passivate() {
  backend().del(operation::read, fd(), this);
}

void event_handler::activate() {
  backend().add(operation::read, fd(), this);
}

void event_handler::set_fd_flags() {
  if (fd_ == invalid_native_socket)
    return;
  // enable nonblocking IO, disable Nagle's algorithm, and suppress SIGPIPE
  nonblocking(fd_, true);
  tcp_nodelay(fd_, true);
  allow_sigpipe(fd_, false);
}

pipe_reader::pipe_reader(default_multiplexer& dm)
    : event_handler(dm, invalid_native_socket) {
  // nop
}

void pipe_reader::removed_from_loop(operation) {
  // nop
}

resumable* pipe_reader::try_read_next() {
  intptr_t ptrval;
  // on windows, we actually have sockets, otherwise we have file handles
# ifdef CAF_WINDOWS
    auto res = recv(fd(), reinterpret_cast<socket_recv_ptr>(&ptrval),
                    sizeof(ptrval), 0);
# else
    auto res = read(fd(), &ptrval, sizeof(ptrval));
# endif
  if (res != sizeof(ptrval))
    return nullptr;
  return reinterpret_cast<resumable*>(ptrval);
}

void pipe_reader::handle_event(operation op) {
  CAF_LOG_TRACE(CAF_ARG(op));
  auto mt = backend().system().config().scheduler_max_throughput;
  switch (op) {
    case operation::read: {
    auto cb = try_read_next();
      switch (cb->resume(&backend(), mt)) {
        case resumable::resume_later:
          backend().exec_later(cb);
          break;
        case resumable::done:
        case resumable::awaiting_message:
          intrusive_ptr_release(cb);
          break;
        default:
          break; // ignored
      }
      break;
    }
    default:
      // nop (simply ignore errors)
      break;
  }
}

void pipe_reader::init(native_socket sock_fd) {
  fd_ = sock_fd;
}

stream::stream(default_multiplexer& backend_ref, native_socket sockfd)
    : event_handler(backend_ref, sockfd),
      read_threshold_(1),
      collected_(0),
      ack_writes_(false),
      writing_(false),
      written_(0) {
  configure_read(receive_policy::at_most(1024));
}

void stream::start(stream_manager* mgr) {
  CAF_ASSERT(mgr != nullptr);
  activate(mgr);
}

void stream::activate(stream_manager* mgr) {
  if (!reader_) {
    reader_.reset(mgr);
    event_handler::activate();
    prepare_next_read();
  }
}

void stream::configure_read(receive_policy::config config) {
  rd_flag_ = config.first;
  max_ = config.second;
}

void stream::ack_writes(bool x) {
  ack_writes_ = x;
}

void stream::write(const void* buf, size_t num_bytes) {
  CAF_LOG_TRACE(CAF_ARG(num_bytes));
  auto first = reinterpret_cast<const char*>(buf);
  auto last  = first + num_bytes;
  wr_offline_buf_.insert(wr_offline_buf_.end(), first, last);
}

void stream::flush(const manager_ptr& mgr) {
  CAF_ASSERT(mgr != nullptr);
  CAF_LOG_TRACE(CAF_ARG(wr_offline_buf_.size()));
  if (!wr_offline_buf_.empty() && !writing_) {
    backend().add(operation::write, fd(), this);
    writer_ = mgr;
    writing_ = true;
    prepare_next_write();
  }
}

void stream::stop_reading() {
  CAF_LOG_TRACE("");
  close_read_channel();
  passivate();
}

void stream::removed_from_loop(operation op) {
  switch (op) {
    case operation::read:  reader_.reset(); break;
    case operation::write: writer_.reset(); break;
    case operation::propagate_error: break;
  }
}

size_t stream::max_consecutive_reads() {
  return backend().system().config().middleman_max_consecutive_reads;
}

void stream::prepare_next_read() {
  collected_ = 0;
  switch (rd_flag_) {
    case receive_policy_flag::exactly:
      if (rd_buf_.size() != max_)
        rd_buf_.resize(max_);
      read_threshold_ = max_;
      break;
    case receive_policy_flag::at_most:
      if (rd_buf_.size() != max_)
        rd_buf_.resize(max_);
      read_threshold_ = 1;
      break;
    case receive_policy_flag::at_least: {
      // read up to 10% more, but at least allow 100 bytes more
      auto max_size = max_ + std::max<size_t>(100, max_ / 10);
      if (rd_buf_.size() != max_size)
        rd_buf_.resize(max_size);
      read_threshold_ = max_;
      break;
    }
  }
}

void stream::prepare_next_write() {
  CAF_LOG_TRACE(CAF_ARG(wr_buf_.size()) << CAF_ARG(wr_offline_buf_.size()));
  written_ = 0;
  wr_buf_.clear();
  if (wr_offline_buf_.empty()) {
    writing_ = false;
    backend().del(operation::write, fd(), this);
  } else {
    wr_buf_.swap(wr_offline_buf_);
  }
}

acceptor::acceptor(default_multiplexer& backend_ref, native_socket sockfd)
    : event_handler(backend_ref, sockfd),
      sock_(invalid_native_socket) {
  // nop
}

void acceptor::start(acceptor_manager* mgr) {
  CAF_LOG_TRACE(CAF_ARG(fd()));
  CAF_ASSERT(mgr != nullptr);
  activate(mgr);
}

void acceptor::activate(acceptor_manager* mgr) {
  if (!mgr_) {
    mgr_.reset(mgr);
    event_handler::activate();
  }
}

void acceptor::stop_reading() {
  CAF_LOG_TRACE(CAF_ARG(fd()));
  close_read_channel();
  passivate();
}

void acceptor::removed_from_loop(operation op) {
  CAF_LOG_TRACE(CAF_ARG(fd()) << CAF_ARG(op));
  if (op == operation::read)
    mgr_.reset();
}

class socket_guard {
public:
  explicit socket_guard(native_socket fd) : fd_(fd) {
    // nop
  }

  ~socket_guard() {
    close();
  }

  native_socket release() {
    auto fd = fd_;
    fd_ = invalid_native_socket;
    return fd;
  }

  void close() {
    if (fd_ != invalid_native_socket) {
      closesocket(fd_);
      fd_ = invalid_native_socket;
    }
  }

private:
  native_socket fd_;
};

auto addr_of(sockaddr_in& what) -> decltype(what.sin_addr)& {
  return what.sin_addr;
}

auto family_of(sockaddr_in& what) -> decltype(what.sin_family)& {
  return what.sin_family;
}

auto port_of(sockaddr_in& what) -> decltype(what.sin_port)& {
  return what.sin_port;
}

auto addr_of(sockaddr_in6& what) -> decltype(what.sin6_addr)& {
  return what.sin6_addr;
}

auto family_of(sockaddr_in6& what) -> decltype(what.sin6_family)& {
  return what.sin6_family;
}

auto port_of(sockaddr_in6& what) -> decltype(what.sin6_port)& {
  return what.sin6_port;
}

auto port_of(sockaddr& what) -> decltype(port_of(std::declval<sockaddr_in&>())) {
  switch (what.sa_family) {
    case AF_INET:
      return port_of(reinterpret_cast<sockaddr_in&>(what));
    case AF_INET6:
      return port_of(reinterpret_cast<sockaddr_in6&>(what));
    default:
      break;
  }
  CAF_CRITICAL("invalid protocol family");
}

template <int Family>
bool ip_connect(native_socket fd, const std::string& host, uint16_t port) {
  CAF_LOG_TRACE("Family =" << (Family == AF_INET ? "AF_INET" : "AF_INET6")
                << CAF_ARG(fd) << CAF_ARG(host));
  static_assert(Family == AF_INET || Family == AF_INET6, "invalid family");
  using sockaddr_type =
    typename std::conditional<
      Family == AF_INET,
      sockaddr_in,
      sockaddr_in6
    >::type;
  sockaddr_type sa;
  memset(&sa, 0, sizeof(sockaddr_type));
  inet_pton(Family, host.c_str(), &addr_of(sa));
  family_of(sa) = Family;
  port_of(sa)   = htons(port);
  return connect(fd, reinterpret_cast<const sockaddr*>(&sa), sizeof(sa)) == 0;
}

expected<native_socket> new_tcp_connection(const std::string& host,
                                           uint16_t port,
                                           optional<protocol> preferred) {
  CAF_LOG_TRACE(CAF_ARG(host) << CAF_ARG(port) << CAF_ARG(preferred));
  CAF_LOG_INFO("try to connect to:" << CAF_ARG(host) << CAF_ARG(port));
  auto res = interfaces::native_address(host, std::move(preferred));
  if (!res) {
    CAF_LOG_INFO("no such host");
    return make_error(sec::cannot_connect_to_node, "no such host", host, port);
  }
  auto proto = res->second;
  CAF_ASSERT(proto == ipv4 || proto == ipv6);
  CALL_CFUN(fd, cc_valid_socket, "socket",
            socket(proto == ipv4 ? AF_INET : AF_INET6, SOCK_STREAM, 0));
  socket_guard sguard(fd);
  if (proto == ipv6) {
    if (ip_connect<AF_INET6>(fd, res->first, port)) {
      CAF_LOG_INFO("successfully connected to host via IPv6");
      return sguard.release();
    }
    sguard.close();
    // IPv4 fallback
    return new_tcp_connection(host, port, ipv4);
  }
  if (!ip_connect<AF_INET>(fd, res->first, port)) {
    CAF_LOG_INFO("could not connect to:" << CAF_ARG(host) << CAF_ARG(port));
    return make_error(sec::cannot_connect_to_node,
                      "ip_connect failed", host, port);
  }
  CAF_LOG_INFO("successfully connected to host via IPv4");
  return sguard.release();
}

template <class SockAddrType>
expected<void> read_port(native_socket fd, SockAddrType& sa) {
  socklen_t len = sizeof(SockAddrType);
  CALL_CFUN(res, cc_zero, "getsockname",
            getsockname(fd, reinterpret_cast<sockaddr*>(&sa), &len));
  return unit;
}

expected<void> set_inaddr_any(native_socket, sockaddr_in& sa) {
  sa.sin_addr.s_addr = INADDR_ANY;
  return unit;
}

expected<void> set_inaddr_any(native_socket fd, sockaddr_in6& sa) {
  sa.sin6_addr = in6addr_any;
  // also accept ipv4 requests on this socket
  int off = 0;
  CALL_CFUN(res, cc_zero, "setsockopt",
            setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY,
                       reinterpret_cast<setsockopt_ptr>(&off),
                       static_cast<socklen_t>(sizeof(off))));
  return unit;
}

template <int Family>
expected<native_socket> new_ip_acceptor_impl(uint16_t port, const char* addr,
                                             bool reuse_addr, bool any) {
  static_assert(Family == AF_INET || Family == AF_INET6, "invalid family");
  CAF_LOG_TRACE(CAF_ARG(port) << ", addr = " << (addr ? addr : "nullptr"));
  CALL_CFUN(fd, cc_valid_socket, "socket", socket(Family, SOCK_STREAM, 0));
  // sguard closes the socket in case of exception
  socket_guard sguard{fd};
  if (reuse_addr) {
    int on = 1;
    CALL_CFUN(tmp1, cc_zero, "setsockopt",
              setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                         reinterpret_cast<setsockopt_ptr>(&on),
                         static_cast<socklen_t>(sizeof(on))));
  }
  using sockaddr_type =
    typename std::conditional<
      Family == AF_INET,
      sockaddr_in,
      sockaddr_in6
    >::type;
  sockaddr_type sa;
  memset(&sa, 0, sizeof(sockaddr_type));
  family_of(sa) = Family;
  if (any)
    set_inaddr_any(fd, sa);
  CALL_CFUN(tmp, cc_one, "inet_pton",
            inet_pton(Family, addr, &addr_of(sa)));
  port_of(sa) = htons(port);
  CALL_CFUN(res, cc_zero, "bind",
            bind(fd, reinterpret_cast<sockaddr*>(&sa),
                 static_cast<socklen_t>(sizeof(sa))));
  return sguard.release();
}

expected<native_socket> new_tcp_acceptor_impl(uint16_t port, const char* addr,
                                              bool reuse_addr) {
  CAF_LOG_TRACE(CAF_ARG(port) << ", addr = " << (addr ? addr : "nullptr"));
  auto addrs = interfaces::server_address(port, addr);
  auto addr_str = std::string{addr == nullptr ? "" : addr};
  if (addrs.empty())
    return make_error(sec::cannot_open_port, "No local interface available",
                      addr_str);
  bool any = addr_str.empty() || addr_str == "::" || addr_str == "0.0.0.0";
  auto fd = invalid_native_socket;
  for (auto& elem : addrs) {
    auto hostname = elem.first.c_str();
    auto p = elem.second == ipv4
           ? new_ip_acceptor_impl<AF_INET>(port, hostname, reuse_addr, any)
           : new_ip_acceptor_impl<AF_INET6>(port, hostname, reuse_addr, any);
    if (!p) {
      CAF_LOG_DEBUG(p.error());
      continue;
    }
    fd = *p;
    break;
  }
  if (fd == invalid_native_socket) {
    CAF_LOG_WARNING("could not open tcp socket on:" << CAF_ARG(port)
                    << CAF_ARG(addr_str));
    return make_error(sec::cannot_open_port, "tcp socket creation failed",
                      port, addr_str);
  }
  socket_guard sguard{fd};
  CALL_CFUN(tmp2, cc_zero, "listen", listen(fd, SOMAXCONN));
  // ok, no errors so far
  CAF_LOG_DEBUG(CAF_ARG(fd));
  return sguard.release();
}

expected<std::string> local_addr_of_fd(native_socket fd) {
  sockaddr_storage st;
  socklen_t st_len = sizeof(st);
  sockaddr* sa = reinterpret_cast<sockaddr*>(&st);
  CALL_CFUN(tmp1, cc_zero, "getsockname", getsockname(fd, sa, &st_len));
  char addr[INET6_ADDRSTRLEN] {0};
  switch (sa->sa_family) {
    case AF_INET:
      return inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(sa)->sin_addr,
                       addr, sizeof(addr));
    case AF_INET6:
      return inet_ntop(AF_INET6,
                       &reinterpret_cast<sockaddr_in6*>(sa)->sin6_addr,
                       addr, sizeof(addr));
    default:
      break;
  }
  return make_error(sec::invalid_protocol_family,
                    "local_addr_of_fd", sa->sa_family);
}

expected<uint16_t> local_port_of_fd(native_socket fd) {
  sockaddr_storage st;
  socklen_t st_len = sizeof(st);
  CALL_CFUN(tmp, cc_zero, "getsockname",
            getsockname(fd, reinterpret_cast<sockaddr*>(&st), &st_len));
  return ntohs(port_of(reinterpret_cast<sockaddr&>(st)));
}

expected<std::string> remote_addr_of_fd(native_socket fd) {
  sockaddr_storage st;
  socklen_t st_len = sizeof(st);
  sockaddr* sa = reinterpret_cast<sockaddr*>(&st);
  CALL_CFUN(tmp, cc_zero, "getpeername", getpeername(fd, sa, &st_len));
  char addr[INET6_ADDRSTRLEN] {0};
  switch (sa->sa_family) {
    case AF_INET:
      return inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(sa)->sin_addr,
                       addr, sizeof(addr));
    case AF_INET6:
      return inet_ntop(AF_INET6,
                       &reinterpret_cast<sockaddr_in6*>(sa)->sin6_addr,
                       addr, sizeof(addr));
    default:
      break;
  }
  return make_error(sec::invalid_protocol_family,
                    "remote_addr_of_fd", sa->sa_family);
}

expected<uint16_t> remote_port_of_fd(native_socket fd) {
  sockaddr_storage st;
  socklen_t st_len = sizeof(st);
  CALL_CFUN(tmp, cc_zero, "getpeername",
            getpeername(fd, reinterpret_cast<sockaddr*>(&st), &st_len));
  return ntohs(port_of(reinterpret_cast<sockaddr&>(st)));
}

// -- default doorman and scribe implementations -------------------------------
  
doorman_impl::doorman_impl(default_multiplexer& mx, native_socket sockfd)
    : doorman(network::accept_hdl_from_socket(sockfd)),
      acceptor_(mx, sockfd) {
  // nop
}

bool doorman_impl::new_connection() {
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

void doorman_impl::stop_reading() {
  CAF_LOG_TRACE("");
  acceptor_.stop_reading();
  detach(&acceptor_.backend(), false);
}

void doorman_impl::launch() {
  CAF_LOG_TRACE("");
  acceptor_.start(this);
}

std::string doorman_impl::addr() const {
  auto x = local_addr_of_fd(acceptor_.fd());
  if (!x)
    return "";
  return std::move(*x);
}

uint16_t doorman_impl::port() const {
  auto x = local_port_of_fd(acceptor_.fd());
  if (!x)
    return 0;
  return *x;
}

void doorman_impl::add_to_loop() {
  acceptor_.activate(this);
}

void doorman_impl::remove_from_loop() {
  acceptor_.passivate();
}

scribe_impl::scribe_impl(default_multiplexer& mx, native_socket sockfd)
    : scribe(network::conn_hdl_from_socket(sockfd)),
      launched_(false),
      stream_(mx, sockfd) {
  // nop
}

void scribe_impl::configure_read(receive_policy::config config) {
  CAF_LOG_TRACE("");
  stream_.configure_read(config);
  if (!launched_)
    launch();
}

void scribe_impl::ack_writes(bool enable) {
  CAF_LOG_TRACE(CAF_ARG(enable));
  stream_.ack_writes(enable);
}

std::vector<char>& scribe_impl::wr_buf() {
  return stream_.wr_buf();
}

std::vector<char>& scribe_impl::rd_buf() {
  return stream_.rd_buf();
}

void scribe_impl::stop_reading() {
  CAF_LOG_TRACE("");
  stream_.stop_reading();
  detach(&stream_.backend(), false);
}

void scribe_impl::flush() {
  CAF_LOG_TRACE("");
  stream_.flush(this);
}

std::string scribe_impl::addr() const {
  auto x = remote_addr_of_fd(stream_.fd());
  if (!x)
    return "";
  return *x;
}

uint16_t scribe_impl::port() const {
  auto x = remote_port_of_fd(stream_.fd());
  if (!x)
    return 0;
  return *x;
}

void scribe_impl::launch() {
  CAF_LOG_TRACE("");
  CAF_ASSERT(!launched_);
  launched_ = true;
  stream_.start(this);
}

void scribe_impl::add_to_loop() {
  stream_.activate(this);
}

void scribe_impl::remove_from_loop() {
  stream_.passivate();
}

} // namespace network
} // namespace io
} // namespace caf
