// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <string>
#include <type_traits>

#include "caf/fwd.hpp"

namespace caf::intrusive {

/// Communicates the state of a consumer to a task queue.
enum class task_result {
  /// The consumer processed the task and is ready to receive the next one.
  resume,
  /// The consumer skipped the task and is ready to receive the next one.
  /// Illegal for consumers of non-cached queues (non-cached queues treat
  /// `skip` and `resume` in the same way).
  skip,
  /// The consumer processed the task but does not accept further tasks.
  stop,
  /// The consumer processed the task but does not accept further tasks and no
  /// subsequent queue shall start a new round.
  stop_all,
};

std::string to_string(task_result);

} // namespace caf::intrusive
