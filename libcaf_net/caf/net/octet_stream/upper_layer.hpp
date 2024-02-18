// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"
#include "caf/net/generic_upper_layer.hpp"

#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"

namespace caf::net::octet_stream {

/// The upper layer requests bytes from the lower layer and consumes raw chunks
/// of data.
class CAF_NET_EXPORT upper_layer : public generic_upper_layer {
public:
  virtual ~upper_layer();

  /// Initializes the upper layer.
  /// @param down A pointer to the lower layer that remains valid for the
  ///             lifetime of the upper layer.
  virtual error start(lower_layer* down) = 0;

  /// Consumes bytes from the lower layer.
  /// @param buffer Available bytes to read.
  /// @param delta Bytes that arrived since last calling this function.
  /// @returns The number of consumed bytes. May be zero if waiting for more
  ///          input or negative to signal an error.
  [[nodiscard]] virtual ptrdiff_t consume(byte_span buffer, byte_span delta)
    = 0;

  /// Called from the lower layer whenever data has been written.
  virtual void written(size_t num_bytes);
};

} // namespace caf::net::octet_stream
