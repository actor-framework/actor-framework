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

#include "caf/actor_clock.hpp"

namespace caf {

// -- constructors, destructors, and assignment operators ----------------------

actor_clock::~actor_clock() {
  // nop
}

// -- observers ----------------------------------------------------------------

actor_clock::time_point actor_clock::now() const noexcept {
  return clock_type::now();
}

actor_clock::duration_type
actor_clock::difference(atom_value, long, time_point t0,
                        time_point t1) const noexcept {
  return t1 - t0;
}

} // namespace caf
