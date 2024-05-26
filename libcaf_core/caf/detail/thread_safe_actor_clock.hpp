// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/action.hpp"
#include "caf/actor_clock.hpp"
#include "caf/actor_control_block.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/sync_ring_buffer.hpp"
#include "caf/fwd.hpp"

#include <memory>
#include <thread>
#include <vector>

namespace caf::detail {

class CAF_CORE_EXPORT thread_safe_actor_clock : public actor_clock {
public:
  // -- constants --------------------------------------------------------------

  static constexpr size_t buffer_size = 64;

  // -- constructors, destructors, and assignment operators --------------------

  thread_safe_actor_clock(actor_system& sys);

  ~thread_safe_actor_clock() override;

  // -- member types -----------------------------------------------------------

  using super = actor_clock;

  /// Stores actions along with their scheduling period.
  struct schedule_entry {
    time_point t;
    action f;
  };

  /// @relates schedule_entry
  using schedule_entry_ptr = std::unique_ptr<schedule_entry>;

  // -- overrides --------------------------------------------------------------

  using super::schedule;

  disposable schedule(time_point abs_time, action f) override;

  // -- thread management ------------------------------------------------------

  void start_dispatch_loop(caf::actor_system& sys);

  void stop_dispatch_loop();

private:
  // -- member types -----------------------------------------------------------
  using queue_type = detail::sync_ring_buffer<schedule_entry_ptr, buffer_size>;

  // -- internal API -----------------------------------------------------------

  static void run(queue_type* queue);

  // -- member variables -------------------------------------------------------

  /// Communication to the dispatcher thread.
  detail::sync_ring_buffer<schedule_entry_ptr, buffer_size> queue_;

  /// Handle to the dispatcher thread.
  std::thread dispatcher_;
};

} // namespace caf::detail
