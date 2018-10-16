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

#include "caf/policy/udp.hpp"

#include "caf/config.hpp"

#ifdef CAF_WINDOWS
# include <winsock2.h>
#else
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
#endif

#include "caf/io/convert.hpp"
#include "caf/logger.hpp"

using caf::io::network::is_error;
using caf::io::network::native_socket;
using caf::io::network::signed_size_type;
using caf::io::network::socket_size_type;

namespace caf {
namespace policy {

bool udp::read_datagram(size_t& result, native_socket fd, void* buf,
                        size_t buf_len, ip_endpoint& ep) {
  CAF_LOG_TRACE(CAF_ARG(fd));
  sockaddr_storage addr;
  memset(&addr, 0, sizeof(sockaddr_storage));
  auto len = static_cast<socket_size_type>(sizeof(sockaddr_storage));
  auto sres = ::recvfrom(fd, static_cast<io::network::socket_recv_ptr>(buf),
                         buf_len, 0, reinterpret_cast<sockaddr*>(&addr), &len);
  if (is_error(sres, true)) {
    CAF_LOG_ERROR("recvfrom returned" << CAF_ARG(sres));
    return false;
  }
  // A value of -1 here means that recvfrom returned "would block"
  if (sres < 0) {
    result = 0;
    return true;
  }
  // Parse address of the sender.
  if (!io::convert(reinterpret_cast<sockaddr&>(addr), protocol::udp, ep)) {
    CAF_LOG_ERROR("recvfrom returned an invalid sender IP endpoint");
    return false;
  }
  if (sres == 0)
    CAF_LOG_DEBUG("Received empty datagram");
  else if (sres > static_cast<signed_size_type>(buf_len))
    CAF_LOG_WARNING("recvfrom cut of message, only received "
                    << CAF_ARG(buf_len) << " of " << CAF_ARG(sres) << " bytes");
  result = static_cast<size_t>(sres);
  return true;
}

bool udp::write_datagram(size_t& result, native_socket fd, void* buf,
                         size_t buf_len, const ip_endpoint& ep) {
  CAF_LOG_TRACE(CAF_ARG(fd) << CAF_ARG(buf_len) << CAF_ARG(ep));
  sockaddr_storage addr;
  if (!io::convert(ep, addr)) {
    CAF_LOG_ERROR("failed to convert ip_endpoint to sockaddr_storage");
    return false;
  }
  auto len = static_cast<socket_size_type>(ep.is_v4() ? sizeof(sockaddr_in) :
                                                        sizeof(sockaddr_in6));
  auto sres = ::sendto(fd, reinterpret_cast<io::network::socket_send_ptr>(buf),
                       buf_len, 0, reinterpret_cast<sockaddr*>(&addr), len);
  if (is_error(sres, true)) {
    CAF_LOG_ERROR("sendto failed:"
                  << CAF_ARG2("last-socket-error",
                              io::network::last_socket_error_as_string()));
    return false;
  }
  result = (sres > 0) ? static_cast<size_t>(sres) : 0;
  return true;
}

} // namespace policy
} // namespace caf
