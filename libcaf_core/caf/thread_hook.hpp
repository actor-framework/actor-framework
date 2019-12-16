/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include "caf/fwd.hpp"

#include "caf/detail/core_export.hpp"

namespace caf {

/// Interface to define thread hooks.
class CAF_CORE_EXPORT thread_hook {
public:
  virtual ~thread_hook();

  /// Called by the actor system once before starting any threads.
  virtual void init(actor_system&) = 0;

  /// Called whenever the actor system has started a new thread.
  /// To access a reference to the started thread use `std::this_thread`.
  /// @warning must the thread-safe
  virtual void thread_started() = 0;

  /// Called whenever a thread is about to quit.
  /// To access a reference to the terminating thread use `std::this_thread`.
  /// @warning must the thread-safe
  virtual void thread_terminates() = 0;
};

} // namespace caf
