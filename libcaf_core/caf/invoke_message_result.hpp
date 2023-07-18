// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"

#include <string>

namespace caf {

/// Stores the result of a message invocation.
enum class invoke_message_result {
  /// Indicates that the actor consumed the message.
  consumed,

  /// Indicates that the actor left the message in the mailbox.
  skipped,

  /// Indicates that the actor discarded the message based on meta data. For
  /// example, timeout messages for already received requests usually get
  /// dropped without calling any user-defined code.
  dropped,
};

CAF_CORE_EXPORT std::string to_string(invoke_message_result);

} // namespace caf
