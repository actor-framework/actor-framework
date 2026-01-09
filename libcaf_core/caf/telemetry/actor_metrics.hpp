// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/fwd.hpp"

namespace caf::telemetry {

/// Optional metrics collected by individual actors when configured to do so.
struct actor_metrics {
  /// Counts the total number of processed messages.
  int_counter* processed_messages = nullptr;

  /// Samples how long the actor needs to process messages.
  dbl_histogram* processing_time = nullptr;

  /// Samples how long messages wait in the mailbox before being processed.
  dbl_histogram* mailbox_time = nullptr;

  /// Counts how many messages are currently waiting in the mailbox.
  int_gauge* mailbox_size = nullptr;

  /// Tracks the current number of running actors of this type.
  int_gauge* running_count = nullptr;
};

} // namespace caf::telemetry
