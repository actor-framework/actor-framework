// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"
#include "caf/net/lp/lower_layer.hpp"
#include "caf/net/lp/upper_layer.hpp"
#include "caf/net/octet_stream/upper_layer.hpp"

#include "caf/detail/net_export.hpp"

#include <memory>

namespace caf::net::lp {

/// Implements length-prefix framing for discretizing a Byte stream into
/// messages of varying size. The framing uses 4 Bytes for the length prefix,
/// but messages (including the 4 Bytes for the length prefix) are limited to a
/// maximum size of INT32_MAX. This limitation comes from the POSIX API (recv)
/// on 32-bit platforms.
class CAF_NET_EXPORT framing : public octet_stream::upper_layer,
                               public lp::lower_layer {
public:
  // -- member types -----------------------------------------------------------

  using upper_layer_ptr = std::unique_ptr<lp::upper_layer>;

  // -- factories --------------------------------------------------------------

  static std::unique_ptr<framing> make(upper_layer_ptr up);

  static disposable run(multiplexer& mpx, stream_socket fd,
                        async::consumer_resource<chunk> pull,
                        async::producer_resource<chunk> push);

  static disposable run(multiplexer& mpx, ssl::connection conn,
                        async::consumer_resource<chunk> pull,
                        async::producer_resource<chunk> push);
};

} // namespace caf::net::lp
