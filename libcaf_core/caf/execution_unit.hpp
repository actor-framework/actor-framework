/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#ifndef CAF_DETAIL_EXECUTION_UNIT_HPP
#define CAF_DETAIL_EXECUTION_UNIT_HPP

namespace caf {

class resumable;

/// Identifies an execution unit, e.g., a worker thread of the scheduler.
class execution_unit {

public:

  virtual ~execution_unit();

  /// Enqueues `ptr` to the job list of the execution unit.
  /// @warning Must only be called from a {@link resumable} currently
  ///          executed by this execution unit.
  virtual void exec_later(resumable* ptr) = 0;

};

} // namespace caf

#endif // CAF_DETAIL_EXECUTION_UNIT_HPP
