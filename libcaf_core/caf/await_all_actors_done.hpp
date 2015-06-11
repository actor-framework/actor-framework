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

#ifndef CAF_AWAIT_ALL_ACTORS_DONE_HPP
#define CAF_AWAIT_ALL_ACTORS_DONE_HPP

#include "caf/detail/singletons.hpp"
#include "caf/detail/actor_registry.hpp"

namespace caf {

/// Blocks execution of this actor until all other actors finished execution.
/// @warning This function will cause a deadlock if called from multiple actors.
/// @warning Do not call this function in cooperatively scheduled actors.
inline void await_all_actors_done() {
  detail::singletons::get_actor_registry()->await_running_count_equal(0);
}

} // namespace caf

#endif // CAF_AWAIT_ALL_ACTORS_DONE_HPP
