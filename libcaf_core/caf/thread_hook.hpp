// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

namespace caf {

/// Interface to define thread hooks.
class CAF_CORE_EXPORT thread_hook {
public:
  virtual ~thread_hook();

  /// Called by the actor system once before starting any threads.
  virtual void init(actor_system&) = 0;

  /// Called whenever the actor system has started a new thread. To access a
  /// reference to the started thread use `std::this_thread`.
  /// @param owner Identifies the CAF component that created this thread.
  /// @warning must the thread-safe
  virtual void thread_started(thread_owner owner) = 0;

  /// Called whenever a thread is about to quit.
  /// To access a reference to the terminating thread use `std::this_thread`.
  /// @warning must the thread-safe
  virtual void thread_terminates() = 0;
};

} // namespace caf
