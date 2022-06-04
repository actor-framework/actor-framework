// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"

namespace caf::net {

/// Groups functions for managing the host system.
class CAF_NET_EXPORT this_host {
public:
  /// Initializes the network subsystem.
  static void startup();

  /// Release any resources of the network subsystem.
  static void cleanup();
};

} // namespace caf::net
