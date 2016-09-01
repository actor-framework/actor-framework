/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#include "caf/detail/message_data.hpp"

#include <cstring>

namespace caf {
namespace detail {

message_data::~message_data() {
  // nop
}

bool message_data::shared() const noexcept {
  return !unique();
}

message_data* message_data::cow_ptr::get_unshared() {
  auto p = ptr_.get();
  if (!p->unique()) {
    auto cptr = p->copy();
    ptr_.swap(cptr.ptr_);
    return ptr_.get();
  }
  return p;
}

} // namespace detail
} // namespace caf
