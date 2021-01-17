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

bool deserializer::fetch_next_object_name(string_view& type_name) {
  auto t = type_id_t{};
  if (fetch_next_object_type(t)) {
    type_name = query_type_name(t);
    return true;
  } else {
    return false;
  }
}

bool deserializer::next_object_name_matches(string_view type_name) {
  string_view found;
  if (fetch_next_object_name(found)) {
    return type_name == found;
  } else {
    return false;
  }
}

bool deserializer::assert_next_object_name(string_view type_name) {
  string_view found;
  if (fetch_next_object_name(found)) {
    if (type_name == found) {
      return true;
    } else {
      std::string str = "required type ";
      str.insert(str.end(), type_name.begin(), type_name.end());
      str += ", got ";
      str.insert(str.end(), found.begin(), found.end());
      emplace_error(sec::type_clash, __func__, std::move(str));
      return false;
    }
  } else {
    emplace_error(sec::runtime_error, __func__, "no type name available");
    return false;
  }
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
