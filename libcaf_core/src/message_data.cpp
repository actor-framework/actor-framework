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

#include "caf/detail/message_data.hpp"

#include <cstring>

namespace caf {
namespace detail {

message_data::~message_data() {
  // nop
}

bool message_data::equals(const message_data& other) const {
  if (this == &other)
    return true;
  auto n = size();
  if (n != other.size())
    return false;
  for (size_t i = 0; i < n; ++i)
    if (! compare_at(i, other.type_at(i), other.at(i)))
      return false;
  return true;
}

uint16_t message_data::type_nr_at(size_t pos) const {
  return type_at(pos).first;
}

message_data* message_data::cow_ptr::get_unshared() {
  auto p = ptr_.get();
  if (! p->unique()) {
    auto cptr = p->copy();
    ptr_.swap(cptr.ptr_);
    return ptr_.get();
  }
  return p;
}

} // namespace detail
} // namespace caf
