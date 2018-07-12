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

#include "caf/logger.hpp"

#ifdef CAF_WINDOWS
# include <winsock2.h>
#else
# include <sys/types.h>
# include <sys/socket.h>
#endif

using caf::io::network::is_error;
using caf::io::network::ip_endpoint;
using caf::io::network::native_socket;
using caf::io::network::signed_size_type;
using caf::io::network::socket_size_type;

namespace caf {
namespace policy {

bool udp::read_datagram(size_t& result, native_socket fd, void* buf,
                        size_t buf_len, ip_endpoint& ep) {
  CAF_LOG_TRACE(CAF_ARG(fd));
  memset(ep.address(), 0, sizeof(sockaddr_storage));
  socket_size_type len = sizeof(sockaddr_storage);
  auto sres = ::recvfrom(fd, static_cast<io::network::socket_recv_ptr>(buf),
                         buf_len, 0, ep.address(), &len);
  if (is_error(sres, true)) {
    CAF_LOG_ERROR("recvfrom returned" << CAF_ARG(sres));
    return false;
  }
  if (sres == 0)
    CAF_LOG_INFO("Received empty datagram");
  else if (sres > static_cast<signed_size_type>(buf_len))
    CAF_LOG_WARNING("recvfrom cut of message, only received "
                    << CAF_ARG(buf_len) << " of " << CAF_ARG(sres) << " bytes");
  result = (sres > 0) ? static_cast<size_t>(sres) : 0;
  *ep.length() = static_cast<size_t>(len);
  return true;
}

bool udp::write_datagram(size_t& result, native_socket fd, void* buf,
                         size_t buf_len, const ip_endpoint& ep) {
  CAF_LOG_TRACE(CAF_ARG(fd) << CAF_ARG(buf_len));
  socket_size_type len = static_cast<socket_size_type>(*ep.clength());
  auto sres = ::sendto(fd, reinterpret_cast<io::network::socket_send_ptr>(buf),
                       buf_len, 0, ep.caddress(), len);
  if (is_error(sres, true)) {
    CAF_LOG_ERROR("sendto returned" << CAF_ARG(sres));
    return false;
  }
  result = (sres > 0) ? static_cast<size_t>(sres) : 0;
  return true;
}

} // namespace policy
} // namespace caf
