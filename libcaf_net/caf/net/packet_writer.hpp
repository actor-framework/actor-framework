/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <vector>

#include "caf/byte.hpp"
#include "caf/net/fwd.hpp"
#include "caf/span.hpp"

namespace caf {
namespace net {

/// Implements an interface for packet writing in application-layers.
class packet_writer {
public:
  using buffer_type = std::vector<byte>;

  virtual ~packet_writer() = default;

  /// Returns the next header_buffer from transport
  virtual buffer_type next_header_buffer() = 0;

  /// Returns the next payload_buffer from transport
  virtual buffer_type next_buffer() = 0;

  /// Convenience function to write a packet consisting of multiple buffers.
  /// @param buffers the buffers that make up the packet. first buffer should
  /// contain the header.
  /// @warning takes ownership of `buffers`.
  template <class... Ts>
  void write_packet(Ts&... buffers) {
    buffer_type* bufs[] = {&buffers...};
    write_impl(make_span(bufs, sizeof...(Ts)));
  }

protected:
  /// Actual write_packet implementation.
  /// @param buffers `span` containing all buffers of a packet.
  virtual void write_impl(span<buffer_type*> buffers) = 0;
};

} // namespace net
} // namespace caf
