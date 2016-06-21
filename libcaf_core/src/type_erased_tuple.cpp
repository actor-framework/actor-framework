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

#include "caf/type_erased_tuple.hpp"

#include "caf/config.hpp"

#include "caf/detail/dynamic_message_data.hpp"

namespace caf {

type_erased_tuple::~type_erased_tuple() {
  // nop
}

void type_erased_tuple::load(deserializer& source) {
  for (size_t i = 0; i < size(); ++i)
    load(i, source);
}

bool type_erased_tuple::empty() const {
  return size() == 0;
}

std::string type_erased_tuple::stringify() const {
  if (size() == 0)
    return "()";
  std::string result = "(";
  result += stringify(0);
  for (size_t i = 1; i < size(); ++i) {
    result += ", ";
    result += stringify(i);
  }
  result += ')';
  return result;
}

void type_erased_tuple::save(serializer& sink) const {
  for (size_t i = 0; i < size(); ++i)
    save(i, sink);
}

bool type_erased_tuple::matches(size_t pos, uint16_t nr,
                                const std::type_info* ptr) const noexcept {
  CAF_ASSERT(pos < size());
  auto tp = type(pos);
  if (tp.first != nr)
    return false;
  if (nr == 0)
    return ptr ? *tp.second == *ptr : false;
  return true;
}

} // namespace caf
