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

#include "caf/scoped_execution_unit.hpp"

#include "caf/actor_system.hpp"
#include "caf/scheduler/abstract_coordinator.hpp"

namespace caf {

scoped_execution_unit::scoped_execution_unit(actor_system* sys)
  : execution_unit(sys){
  // nop
}

void scoped_execution_unit::exec_later(resumable* ptr, bool) {
  system().scheduler().enqueue(ptr);
}

bool scoped_execution_unit::is_neighbor(execution_unit*) const {
  return false;
}

} // namespace caf
