// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor_control_block.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/spawn_options.hpp"

namespace caf {

/// Utility function object that allows users to explicitly launch an actor by
/// calling `operator()`. Launches the actor implicitly at scope exit if the
/// user did not launch it explicitly.
class CAF_CORE_EXPORT actor_launcher {
public:
  actor_launcher() noexcept : ready_(false) {
    // nop
  }

  actor_launcher(strong_actor_ptr self, scheduler* context,
                 spawn_options options);

  actor_launcher(actor_launcher&& other) noexcept;

  actor_launcher& operator=(actor_launcher&& other) noexcept;

  actor_launcher(const actor_launcher&) = delete;

  actor_launcher& operator=(const actor_launcher&) = delete;

  ~actor_launcher();

  void operator()();

private:
  struct state {
    strong_actor_ptr self;
    scheduler* context;
    spawn_options options;
  };

  // @pre `ready == true`
  void reset();

  bool ready_;
  union {
    state state_;
  };
};

} // namespace caf
