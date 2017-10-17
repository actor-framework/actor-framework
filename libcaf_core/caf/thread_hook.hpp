/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_THREAD_HOOK_HPP
#define CAF_THREAD_HOOK_HPP

namespace caf {

/// Interface to define thread hooks.
class thread_hook {
public:
  virtual ~thread_hook() {
    // nop
  }

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

#endif // CAF_THREAD_HOOK_HPP
