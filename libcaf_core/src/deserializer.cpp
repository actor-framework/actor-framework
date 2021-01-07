// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

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

bool deserializer::list(std::vector<bool>& x) {
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
