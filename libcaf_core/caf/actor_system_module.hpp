// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

namespace caf {

/// An (optional) component of the actor system.
class CAF_CORE_EXPORT actor_system_module {
public:
  enum id_t { middleman, openssl_manager, network_manager, daemons, num_ids };

  virtual ~actor_system_module();

  /// Returns the human-readable name of the module.
  const char* name() const noexcept;

  /// Starts any background threads needed by the module.
  virtual void start() = 0;

  /// Stops all background threads of the module.
  virtual void stop() = 0;

  /// Allows the module to change the
  /// configuration of the actor system during startup.
  virtual void init(actor_system_config&) = 0;

  /// Returns the identifier of this module.
  virtual id_t id() const = 0;

  /// Returns a pointer to the subtype.
  virtual void* subtype_ptr() = 0;
};

} // namespace caf
