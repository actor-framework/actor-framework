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


#ifndef CAF_SCHEDULER_DETACHED_THREADS_HPP
#define CAF_SCHEDULER_DETACHED_THREADS_HPP

#include <cstddef>

namespace caf {
namespace scheduler {

/// Increases count for detached threads by one.
void inc_detached_threads();

/// Decreases count for detached threads by one.
void dec_detached_threads();

/// Blocks the caller until all detached threads are done.
void await_detached_threads();

} // namespace scheduler
} // namespace caf

#endif // CAF_SCHEDULER_DETACHED_THREADS_HPP
