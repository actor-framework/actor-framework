// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/logger.hpp"

namespace caf::detail {

/// Logger interface that adds lifecycle methods for use by the actor system.
/// Implementations may start background threads in start() and must stop them
/// in stop().
class CAF_CORE_EXPORT asynchronous_logger : public logger {
public:
  using logger::logger;

  /// Starts any background threads needed by the logger.
  virtual void start() = 0;

  /// Stops all background threads of the logger.
  virtual void stop() = 0;

  /// Creates a new asynchronous logger instance.
  static intrusive_ptr<asynchronous_logger> make(actor_system& sys);
};

} // namespace caf::detail
