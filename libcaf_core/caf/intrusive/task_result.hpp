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

#pragma once

#include <type_traits>

#include "caf/fwd.hpp"

namespace caf {
namespace intrusive {

/// Communicates the state of a consumer to a task queue.
enum class task_result {
  /// The consumer processed the task and is ready to receive the next one.
  resume,
  /// The consumer skipped the task and is ready to receive the next one.
  /// Illegal for consumers of non-cached queues (non-cached queues treat
  /// `skip` and `resume` in the same way).
  skip,
  /// The consumer processed the task but does not accept further tasks.
  stop,
  /// The consumer processed the task but does not accept further tasks and no
  /// subsequent queue shall start a new round.
  stop_all,
};

} // namespace intrusive
} // namespace caf

