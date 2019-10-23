/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
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

#include "caf/detail/build_config.hpp"
#include "caf/fwd.hpp"

namespace caf {

/// A profiler which provides a set of callbacks for several actor operations in
/// order to collect fine-grained profiling state about the system.
/// @experimental
class actor_profiler {
public:
  virtual ~actor_profiler();

  /// Called whenever the actor system spawns a new actor. The system calls this
  /// member function after the constructor of `self` has completed but before
  /// constructing the behavior .
  /// @param self The new actor.
  /// @param parent Points to the parent actor unless `self` is a top-level
  ///               actor (in this case, `parent` has the value `nullptr`).
  /// @thread-safe
  virtual void add_actor(const local_actor& self, const local_actor* parent)
    = 0;

  /// Called before the actor system calls the destructor for `self`.
  /// @param ptr Points to an actor that is about to get destroyed.
  /// @thread-safe
  virtual void remove_actor(const local_actor& self) = 0;

  /// Called whenever an actor is about to process an element from its mailbox.
  /// @param self The current actor.
  /// @param element The current element from the mailbox.
  /// @thread-safe
  virtual void before_processing(const local_actor& self,
                                 const mailbox_element& element)
    = 0;

  /// Called whenever an actor processed an element from its mailbox.
  /// @param self The current actor.
  /// @param result Stores whether the actor consumed, skipped or dropped the
  ///               message.
  /// @thread-safe
  virtual void after_processing(const local_actor& self,
                                invoke_message_result result)
    = 0;
};

#ifdef CAF_ENABLE_ACTOR_PROFILER
#  define CAF_BEFORE_PROCESSING(self, msg)                                     \
    self->system().profiler_before_processing(*self, msg)
#  define CAF_AFTER_PROCESSING(self, result)                                   \
    self->system().profiler_after_processing(*self, result)
#else
#  define CAF_BEFORE_PROCESSING(self, msg) CAF_VOID_STMT
#  define CAF_AFTER_PROCESSING(self, result) CAF_VOID_STMT
#endif

} // namespace caf
