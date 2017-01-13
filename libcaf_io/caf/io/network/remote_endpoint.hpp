/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#ifndef CAF_IO_REMOTE_ENDPOINT_HPP
#define CAF_IO_REMOTE_ENDPOINT_HPP

#include <deque>
#include <vector>

namespace caf {
namespace io {
namespace network {

class remote_endpoint {
  /// A buffer class providing a compatible
  /// interface to `std::vector`.
  using buffer_type = std::vector<char>;

private:
  // state for receiving
  size_t dgram_size_;
  buffer_type rd_buf_;
  size_t bytes_read_;

  // state for sending
  bool ack_writes_;
  bool writing_;
  buffer_type wr_buf_;
  std::deque<buffer_type> wr_offline_buf_;

  // endpoint info
  struct sockaddr_storage remote_endpoint_addr_;
  socklen_t remote_endpoint_addr_len_;
};

} // namespace network
} // namespace io
} // namespace caf

#endif // CAF_IO_REMOTE_ENDPOINT_HPP
