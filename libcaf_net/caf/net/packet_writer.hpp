// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"
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
