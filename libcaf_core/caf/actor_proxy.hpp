// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/abstract_actor.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

#include <atomic>
#include <cstdint>

namespace caf {

/// Represents an actor running on a remote machine,
/// or different hardware, or in a separate process.
class CAF_CORE_EXPORT actor_proxy : public abstract_actor {
public:
  explicit actor_proxy(actor_config& cfg);

  ~actor_proxy() override;

  /// Invokes cleanup code.
  virtual void kill_proxy(scheduler* sched, error reason) = 0;

  void setup_metrics() {
    // nop
  }
};

} // namespace caf
