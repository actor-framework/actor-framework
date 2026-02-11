// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor_clock.hpp"
#include "caf/fwd.hpp"

#include <memory>

namespace caf::detail {

/// Actor clock interface that adds lifecycle methods for use by the actor
/// system.  Implementations may start background threads in start() and must
/// stop them in stop().
class CAF_CORE_EXPORT asynchronous_actor_clock : public actor_clock {
public:
  ~asynchronous_actor_clock() override;

  /// Starts any background threads needed by the actor clock.
  virtual void start(caf::actor_system& sys) = 0;

  /// Stops all background threads of the actor clock.
  virtual void stop() = 0;

  /// Creates a new asynchronous actor clock instance.
  static std::unique_ptr<asynchronous_actor_clock>
  make(telemetry::int_gauge* queue_size);
};

} // namespace caf::detail
