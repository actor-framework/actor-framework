// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"
#include "caf/net/octet_stream/lower_layer.hpp"
#include "caf/net/octet_stream/policy.hpp"
#include "caf/net/octet_stream/upper_layer.hpp"
#include "caf/net/socket_event_layer.hpp"
#include "caf/net/stream_socket.hpp"

#include "caf/byte_buffer.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"

namespace caf::net::octet_stream {

/// A transport layer for sending and receiving octet streams.
class CAF_NET_EXPORT transport : public socket_event_layer,
                                 public octet_stream::lower_layer {
public:
  // -- member types -----------------------------------------------------------

  using connection_handle = stream_socket;

  /// An owning smart pointer type for storing an upper layer object.
  using upper_layer_ptr = std::unique_ptr<octet_stream::upper_layer>;

  // -- factories --------------------------------------------------------------

  static std::unique_ptr<transport> make(std::unique_ptr<policy> policy,
                                         upper_layer_ptr up);

  static std::unique_ptr<transport> make(stream_socket fd, upper_layer_ptr up);

  // -- properties -------------------------------------------------------------

  virtual policy& active_policy() = 0;

  virtual size_t max_consecutive_reads() const noexcept = 0;

  virtual void max_consecutive_reads(size_t value) noexcept = 0;
};

} // namespace caf::net::octet_stream
