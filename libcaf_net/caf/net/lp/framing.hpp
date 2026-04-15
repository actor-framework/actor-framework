// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"
#include "caf/net/lp/lower_layer.hpp"
#include "caf/net/lp/size_field_type.hpp"
#include "caf/net/lp/upper_layer.hpp"
#include "caf/net/octet_stream/upper_layer.hpp"

#include "caf/detail/net_export.hpp"

#include <memory>

namespace caf::net::lp {

/// Implements length-prefix framing for discretizing a byte stream into
/// messages of varying size.
///
/// The framing prepends each message with a configurable unsigned size field
/// (see `size_field_type`) and enforces a configurable maximum payload size.
class CAF_NET_EXPORT framing : public octet_stream::upper_layer,
                               public lp::lower_layer {
public:
  // -- member types -----------------------------------------------------------

  using upper_layer_ptr = std::unique_ptr<lp::upper_layer>;

  // -- factories --------------------------------------------------------------

  /// Creates a new framing protocol with CAF 1.x compatible framing defaults.
  ///
  /// Uses `size_field_type::u4` and `caf::defaults::net::lp_max_message_size`
  /// as maximum payload size.
  static std::unique_ptr<framing> make(upper_layer_ptr up);

  /// Creates a new framing protocol with custom framing options.
  static std::unique_ptr<framing>
  make(upper_layer_ptr up, size_field_type size_field, size_t max_message_size);

  static disposable run(multiplexer& mpx, stream_socket fd,
                        async::consumer_resource<chunk> pull,
                        async::producer_resource<chunk> push,
                        size_field_type size_field, size_t max_message_size);

  static disposable run(multiplexer& mpx, ssl::connection conn,
                        async::consumer_resource<chunk> pull,
                        async::producer_resource<chunk> push,
                        size_field_type size_field, size_t max_message_size);
};

} // namespace caf::net::lp
