/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

#ifndef CAF_RESUME_POLICY_HPP
#define CAF_RESUME_POLICY_HPP

#include "caf/resumable.hpp"

// this header consists all type definitions needed to
// implement the resume_policy trait

namespace caf {

class execution_unit;
class duration;

namespace policy {

/**
 * The resume_policy <b>concept</b> class. Please note that this
 * class is <b>not</b> implemented. It only explains the all
 * required member function and their behavior for any resume policy.
 */
class resume_policy {
 public:
  /**
   * Resumes the actor by reading a new message and then
   * calling `self->invoke(msg)`. This process is repeated
   * until either no message is left in the actor's mailbox or the
   * actor finishes execution.
   */
  template <class Actor>
  resumable::resume_result resume(Actor* self, execution_unit*);

  /**
   * Waits unconditionally until the actor is ready to be resumed.
   * This member function calls `self->await_data()`.
   * @note This member function must raise a compiler error if the resume
   *       strategy cannot be used to implement blocking actors.
   */
  template <class Actor>
  bool await_ready(Actor* self);
};

} // namespace policy
} // namespace caf

#endif // CAF_RESUME_POLICY_HPP
