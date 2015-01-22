/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

namespace caf {
namespace detail {

message_data::message_data() {
  // nop
}

bool message_data::equals(const message_data& other) const {
  auto full_eq = [](const message_iterator& lhs, const message_iterator& rhs) {
    return lhs.type() == rhs.type()
        && lhs.type()->equals(lhs.value(), rhs.value());
  };
  return this == &other
         || (size() == other.size()
             && std::equal(begin(), end(), other.begin(), full_eq));
}

const void* message_data::native_data() const {
  return nullptr;
}

void* message_data::mutable_native_data() {
  return nullptr;
}

std::string get_tuple_type_names(const detail::message_data& tup) {
  std::string result = "@<>";
  for (size_t i = 0; i < tup.size(); ++i) {
    auto uti = tup.type_at(i);
    result += "+";
    result += uti->name();
  }
  return result;
}

message_data* message_data::ptr::get_detached() {
  auto p = m_ptr.get();
  if (!p->unique()) {
    auto np = p->copy();
    m_ptr.reset(np);
    return np;
  }
  return p;
}

} // namespace detail
} // namespace caf
