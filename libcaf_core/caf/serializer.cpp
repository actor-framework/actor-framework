// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/serializer.hpp"

#include "caf/actor_system.hpp"

namespace caf {

serializer::serializer(actor_system& sys) noexcept
  : context_(sys.dummy_execution_unit()) {
  // nop
}

serializer::serializer(execution_unit* ctx) noexcept : context_(ctx) {
  // nop
}

serializer::~serializer() {
  // nop
}

bool serializer::begin_key_value_pair() {
  return begin_tuple(2);
}

bool serializer::end_key_value_pair() {
  return end_tuple();
}

bool serializer::begin_associative_array(size_t size) {
  return begin_sequence(size);
}

bool serializer::end_associative_array() {
  return end_sequence();
}

bool serializer::list(const std::vector<bool>& xs) {
  if (!begin_sequence(xs.size()))
    return false;
  for (bool x : xs)
    if (!value(x))
      return false;
  return end_sequence();
}

} // namespace caf
