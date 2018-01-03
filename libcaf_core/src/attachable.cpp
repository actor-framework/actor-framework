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

#include "caf/attachable.hpp"

namespace caf {

attachable::~attachable() {
  // Avoid recursive cleanup of next pointers because this can cause a stack
  // overflow for long linked lists.
  using std::swap;
  while (next != nullptr) {
    attachable_ptr tmp;
    swap(next->next, tmp);
    swap(next, tmp);
  }
}

attachable::token::token(size_t typenr, const void* vptr)
    : subtype(typenr), ptr(vptr) {
  // nop
}

void attachable::actor_exited(const error&, execution_unit*) {
  // nop
}

bool attachable::matches(const token&) {
  return false;
}

} // namespace caf
