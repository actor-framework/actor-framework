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

#include "caf/attachable.hpp"

namespace caf {

attachable::~attachable() {
  // nop
}

attachable::token::token(size_t typenr, const void* vptr)
    : subtype(typenr), ptr(vptr) {
  // nop
}

optional<uint32_t> attachable::handle_exception(const std::exception_ptr&) {
  return none;
}

void attachable::actor_exited(abstract_actor*, uint32_t) {
  // nop
}

bool attachable::matches(const token&) {
  return false;
}

} // namespace caf
