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

#include "caf/byte.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/net/fwd.hpp"
#include "caf/span.hpp"

namespace caf::net {

/// Implements an interface for packet writing in application-layers.
class CAF_NET_EXPORT packet_writer {
public:
  virtual ~packet_writer();

  /// Returns a buffer for writing header information.
  virtual byte_buffer next_header_buffer() = 0;

  /// Returns a buffer for writing payload content.
  virtual byte_buffer next_payload_buffer() = 0;

  /// Convenience function to write a packet consisting of multiple buffers.
  /// @param buffers all buffers for the packet. The first buffer is a header
  ///                buffer, the other buffers are payload buffer.
  /// @warning this function takes ownership of `buffers`.
  template <class... Ts>
  void write_packet(Ts&... buffers) {
    byte_buffer* bufs[] = {&buffers...};
    write_impl(make_span(bufs, sizeof...(Ts)));
  }

protected:
  /// Implementing function for `write_packet`.
  /// @param buffers a `span` containing all buffers of a packet.
  virtual void write_impl(span<byte_buffer*> buffers) = 0;
};

} // namespace caf::net
