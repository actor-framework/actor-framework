// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/serializer.hpp"

#include "caf/actor_control_block.hpp"
#include "caf/actor_handle_codec.hpp"
#include "caf/format_to_error.hpp"
#include "caf/type_id.hpp"
#include "caf/type_id_list.hpp"

namespace caf {

serializer::~serializer() {
  // nop
}

std::string_view serializer::to_type_name(type_id_t id) const {
  return query_type_name(id);
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

bool serializer::value(const strong_actor_ptr& ptr) {
  if (auto* codec = actor_handle_codec())
    return codec->save(*this, ptr);
  set_error(make_error(sec::no_actor_handle_codec));
  return false;
}

bool serializer::value(const weak_actor_ptr& ptr) {
  auto tmp = ptr.lock();
  return value(tmp);
}

bool serializer::value(const std::vector<bool>& xs) {
  if (!begin_sequence(xs.size()))
    return false;
  for (bool x : xs)
    if (!value(x))
      return false;
  return end_sequence();
}

bool serializer::value(type_id_list xs) {
  if (!begin_sequence(xs.size()))
    return false;
  for (auto id : xs) {
    auto tname = to_type_name(id);
    if (tname.empty()) {
      set_error(format_to_error(sec::runtime_error,
                                "failed to get type name for type ID {}", id));
      return false;
    }
    if (!value(tname))
      return false;
  }
  return end_sequence();
}

} // namespace caf
