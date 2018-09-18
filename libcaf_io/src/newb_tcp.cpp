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

#include "caf/policy/newb_tcp.hpp"
#include "caf/io/newb.hpp"

#include "caf/config.hpp"

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
# include <io.h>
# include <windows.h>
# include <winsock2.h>
# include <ws2ipdef.h>
# include <ws2tcpip.h>
#else
# include <unistd.h>
# include <arpa/inet.h>
# include <cerrno>
# include <fcntl.h>
# include <netdb.h>
# include <netinet/in.h>
# include <netinet/ip.h>
# include <netinet/tcp.h>
# include <sys/socket.h>
# include <sys/types.h>
# ifdef CAF_POLL_MULTIPLEXER
#   include <poll.h>
# elif defined(CAF_EPOLL_MULTIPLEXER)
#   include <sys/epoll.h>
# else
#   error "neither CAF_POLL_MULTIPLEXER nor CAF_EPOLL_MULTIPLEXER defined"
# endif
#endif

namespace caf {
namespace policy {

tcp_transport::tcp_transport()
    : read_threshold{1},
      collected{0},
      maximum{0},
      writing{false},
      written{0} {
  configure_read(io::receive_policy::at_most(1024));
}

io::network::rw_state tcp_transport::read_some(io::newb_base* parent) {
  CAF_LOG_TRACE("");
  size_t len = receive_buffer.size() - collected;
  void* buf = receive_buffer.data() + collected;
  auto sres = ::recv(parent->fd(),
                     reinterpret_cast<io::network::socket_recv_ptr>(buf),
                     len, io::network::no_sigpipe_io_flag);
  if (sres < 0) {
    auto err = io::network::last_socket_error();
    if (io::network::would_block_or_temporarily_unavailable(err))
      return io::network::rw_state::indeterminate;
    CAF_LOG_DEBUG("recv failed" << CAF_ARG(sres));
    return io::network::rw_state::failure;
  } else if (sres == 0) {
    // Recv returns 0 when the peer has performed an orderly shutdown.
    CAF_LOG_DEBUG("peer shutdown");
    return io::network::rw_state::failure;
  }
  size_t result = (sres > 0) ? static_cast<size_t>(sres) : 0;
  collected += result;
  received_bytes = collected;
  return io::network::rw_state::success;
}

bool tcp_transport::should_deliver() {
  CAF_LOG_DEBUG(CAF_ARG(collected) << CAF_ARG(read_threshold));
  return collected >= read_threshold;
}

void tcp_transport::prepare_next_read(io::newb_base*) {
  collected = 0;
  received_bytes = 0;
  switch (rd_flag) {
    case io::receive_policy_flag::exactly:
      if (receive_buffer.size() != maximum)
        receive_buffer.resize(maximum);
      read_threshold = maximum;
      break;
    case io::receive_policy_flag::at_most:
      if (receive_buffer.size() != maximum)
        receive_buffer.resize(maximum);
      read_threshold = 1;
      break;
    case io::receive_policy_flag::at_least: {
      // Read up to 10% more, but at least allow 100 bytes more.
      auto maximumsize = maximum + std::max<size_t>(100, maximum / 10);
      if (receive_buffer.size() != maximumsize)
        receive_buffer.resize(maximumsize);
      read_threshold = maximum;
      break;
    }
  }
}

void tcp_transport::configure_read(io::receive_policy::config config) {
  rd_flag = config.first;
  maximum = config.second;
}

io::network::rw_state tcp_transport::write_some(io::newb_base* parent) {
  CAF_LOG_TRACE("");
  const void* buf = send_buffer.data() + written;
  auto len = send_buffer.size() - written;
  auto sres = ::send(parent->fd(),
                     reinterpret_cast<io::network::socket_send_ptr>(buf),
                     len, io::network::no_sigpipe_io_flag);
  if (io::network::is_error(sres, true)) {
    CAF_LOG_ERROR("send failed"
                  << CAF_ARG(io::network::last_socket_error_as_string()));
    return io::network::rw_state::failure;
  }
  size_t result = (sres > 0) ? static_cast<size_t>(sres) : 0;
  written += result;
  auto remaining = send_buffer.size() - written;
  if (remaining == 0)
    prepare_next_write(parent);
  return io::network::rw_state::success;
}

void tcp_transport::prepare_next_write(io::newb_base* parent) {
  written = 0;
  send_buffer.clear();
  if (offline_buffer.empty()) {
    parent->stop_writing();
    writing = false;
  } else {
    send_buffer.swap(offline_buffer);
  }
}

void tcp_transport::flush(io::newb_base* parent) {
  CAF_ASSERT(parent != nullptr);
  CAF_LOG_TRACE(CAF_ARG(offline_buffer.size()));
  if (!offline_buffer.empty() && !writing) {
    parent->start_writing();
    writing = true;
    prepare_next_write(parent);
  }
}

expected<io::network::native_socket>
tcp_transport::connect(const std::string& host, uint16_t port,
                       optional<io::network::protocol::network> preferred) {
  return io::network::new_tcp_connection(host, port, preferred);
}

expected<io::network::native_socket>
accept_tcp::create_socket(uint16_t port, const char* host, bool reuse) {
  return io::network::new_tcp_acceptor_impl(port, host, reuse);
}

std::pair<io::network::native_socket, transport_ptr>
accept_tcp::accept_event(io::newb_base* parent) {
  using namespace io::network;
  sockaddr_storage addr;
  std::memset(&addr, 0, sizeof(addr));
  socket_size_type addrlen = sizeof(addr);
  auto result = ::accept(parent->fd(), reinterpret_cast<sockaddr*>(&addr),
                         &addrlen);
  if (result == invalid_native_socket) {
    auto err = last_socket_error();
    if (!would_block_or_temporarily_unavailable(err)) {
      return {invalid_native_socket, nullptr};
    }
  }
  transport_ptr ptr{new tcp_transport};
  return {result, std::move(ptr)};
}

void accept_tcp::init(io::newb_base& n) {
  n.start();
}

} // namespace policy
} // namespace caf
