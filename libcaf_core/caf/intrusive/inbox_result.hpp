// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"

#include <string>

namespace caf::intrusive {

/// Communicates the state of a LIFO or FIFO inbox after pushing to it.
enum class inbox_result {
  /// Indicates that the enqueue operation succeeded and
  /// the reader is ready to receive the data.
  success,

  /// Indicates that the enqueue operation succeeded and
  /// the reader is currently blocked, i.e., needs to be re-scheduled.
  unblocked_reader,

  /// Indicates that the enqueue operation failed because the
  /// queue has been closed by the reader.
  queue_closed,
};

CAF_CORE_EXPORT std::string to_string(inbox_result);

} // namespace caf::intrusive
