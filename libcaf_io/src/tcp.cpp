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

#include "caf/policy/tcp.hpp"

#include <cstring>

#include "caf/logger.hpp"

#ifdef CAF_WINDOWS
# include <winsock2.h>
#else
# include <sys/types.h>
# include <sys/socket.h>
#endif

using caf::io::network::is_error;
using caf::io::network::rw_state;
using caf::io::network::native_socket;
using caf::io::network::socket_size_type;
using caf::io::network::no_sigpipe_io_flag;

namespace caf {
namespace policy {

rw_state tcp::read_some(size_t& result, native_socket fd, void* buf,
                        size_t len) {
  CAF_LOG_TRACE(CAF_ARG(fd) << CAF_ARG(len));
  auto sres = ::recv(fd, reinterpret_cast<io::network::socket_recv_ptr>(buf),
                     len, no_sigpipe_io_flag);
  CAF_LOG_DEBUG(CAF_ARG(len) << CAF_ARG(fd) << CAF_ARG(sres));
  if (is_error(sres, true) || sres == 0) {
    // recv returns 0  when the peer has performed an orderly shutdown
    return rw_state::failure;
  }
  result = (sres > 0) ? static_cast<size_t>(sres) : 0;
  return rw_state::success;
}

rw_state tcp::write_some(size_t& result, native_socket fd, const void* buf,
                         size_t len) {
  CAF_LOG_TRACE(CAF_ARG(fd) << CAF_ARG(len));
  auto sres = ::send(fd, reinterpret_cast<io::network::socket_send_ptr>(buf),
                     len, no_sigpipe_io_flag);
  CAF_LOG_DEBUG(CAF_ARG(len) << CAF_ARG(fd) << CAF_ARG(sres));
  if (is_error(sres, true))
    return rw_state::failure;
  result = (sres > 0) ? static_cast<size_t>(sres) : 0;
  return rw_state::success;
}

bool tcp::try_accept(native_socket& result, native_socket fd) {
  using namespace io::network;
  CAF_LOG_TRACE(CAF_ARG(fd));
  sockaddr_storage addr;
  std::memset(&addr, 0, sizeof(addr));
  socket_size_type addrlen = sizeof(addr);
  result = ::accept(fd, reinterpret_cast<sockaddr*>(&addr), &addrlen);
  // note accept4 is better to avoid races in setting CLOEXEC (but not posix)
  child_process_inherit(result, false);
  CAF_LOG_DEBUG(CAF_ARG(fd) << CAF_ARG(result));
  if (result == invalid_native_socket) {
    auto err = last_socket_error();
    if (!would_block_or_temporarily_unavailable(err)) {
      return false;
    }
  }
  return true;
}

} // namespace policy
} // namespace caf
