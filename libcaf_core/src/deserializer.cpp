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

#include "caf/deserializer.hpp"

#include "caf/actor_system.hpp"

namespace caf {

deserializer::deserializer(actor_system& x) noexcept
  : context_(x.dummy_execution_unit()) {
  // nop
}

deserializer::deserializer(execution_unit* x) noexcept : context_(x) {
  // nop
}

deserializer::~deserializer() {
  // nop
}

bool deserializer::begin_key_value_pair() {
  return begin_tuple(2);
}

bool deserializer::end_key_value_pair() {
  return end_tuple();
}

bool deserializer::begin_associative_array(size_t& size) {
  return begin_sequence(size);
}

bool deserializer::end_associative_array() {
  return end_sequence();
}

bool deserializer::value(std::vector<bool>& x) {
  x.clear();
  size_t size = 0;
  if (!begin_sequence(size))
    return false;
  for (size_t i = 0; i < size; ++i) {
    bool tmp = false;
    if (!value(tmp))
      return false;
    x.emplace_back(tmp);
  }
  return end_sequence();
}

} // namespace caf
