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

#ifndef CAF_SET_SCHEDULER_HPP
#define CAF_SET_SCHEDULER_HPP

#include <thread>
#include <limits>

#include "caf/policy/work_stealing.hpp"

#include "caf/scheduler/worker.hpp"
#include "caf/scheduler/coordinator.hpp"
#include "caf/scheduler/abstract_coordinator.hpp"

namespace caf {
/// Sets a user-defined scheduler.
/// @note This function must be used before actor is spawned. Dynamically
///       changing the scheduler at runtime is not supported.
/// @throws std::logic_error if a scheduler is already defined
void set_scheduler(scheduler::abstract_coordinator* ptr);

/// Sets a user-defined scheduler using given policies. The scheduler
/// is instantiated with `nw` number of workers and allows each actor
/// to consume up to `max_throughput` per resume (must be > 0).
/// @note This function must be used before actor is spawned. Dynamically
///       changing the scheduler at runtime is not supported.
/// @throws std::logic_error if a scheduler is already defined
/// @throws std::invalid_argument if `max_throughput == 0`
template <class Policy = policy::work_stealing>
void set_scheduler(size_t nw = std::thread::hardware_concurrency(),
                   size_t max_throughput = std::numeric_limits<size_t>::max()) {
  if (max_throughput == 0) {
    throw std::invalid_argument("max_throughput must not be 0");
  }
  set_scheduler(new scheduler::coordinator<Policy>(nw, max_throughput));
}

} // namespace caf

#endif // CAF_SET_SCHEDULER_HPP
