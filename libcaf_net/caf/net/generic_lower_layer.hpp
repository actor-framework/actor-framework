// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/net_export.hpp"

namespace caf::net {

/// Bundles protocol-independent, generic member functions for (almost all)
/// lower layers.
class CAF_NET_EXPORT generic_lower_layer {
public:
  virtual ~generic_lower_layer();

  /// Queries whether the output device can accept more data straight away.
  [[nodiscard]] virtual bool can_send_more() const noexcept = 0;

  /// Halt receiving of additional bytes or messages.
  virtual void suspend_reading() = 0;

  /// Queries whether the lower layer is currently configured to halt receiving
  /// of additional bytes or messages.
  [[nodiscard]] virtual bool stopped_reading() const noexcept = 0;
};

} // namespace caf::net
