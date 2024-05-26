// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/log_level.hpp"

namespace caf::log {

/// Provides integer constants for the predefined log levels.
struct level {
  /// Integer value for the 'quiet' log level.
  static constexpr unsigned quiet = CAF_LOG_LEVEL_QUIET;

  /// Integer value for the 'error' log level.
  static constexpr unsigned error = CAF_LOG_LEVEL_ERROR;

  /// Integer value for the 'warning' log level.
  static constexpr unsigned warning = CAF_LOG_LEVEL_WARNING;

  /// Integer value for the 'info' log level.
  static constexpr unsigned info = CAF_LOG_LEVEL_INFO;

  /// Integer value for the 'debug' log level.
  static constexpr unsigned debug = CAF_LOG_LEVEL_DEBUG;

  /// Integer value for the 'trace' log level.
  static constexpr unsigned trace = CAF_LOG_LEVEL_TRACE;
};

} // namespace caf::log
