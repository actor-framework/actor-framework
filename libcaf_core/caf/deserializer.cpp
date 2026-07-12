// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/deserializer.hpp"

#include "caf/actor_control_block.hpp"
#include "caf/actor_handle_codec.hpp"
#include "caf/detail/type_id_list_builder.hpp"
#include "caf/format_to_error.hpp"
#include "caf/sec.hpp"
#include "caf/type_id_list.hpp"

#include <limits>
#include <type_traits>

namespace caf {

deserializer::~deserializer() {
  // nop
}

type_id_t deserializer::to_type_id(std::string_view type_name) const {
  return query_type_id(type_name);
}

bool deserializer::fetch_next_object_name(std::string_view& type_name) {
  auto t = type_id_t{};
  if (fetch_next_object_type(t)) {
    type_name = query_type_name(t);
    return true;
  } else {
    return false;
  }
}

bool deserializer::next_object_name_matches(std::string_view type_name) {
  std::string_view found;
  if (fetch_next_object_name(found)) {
    return type_name == found;
  } else {
    return false;
  }
}

bool deserializer::assert_next_object_name(std::string_view type_name) {
  std::string_view found;
  if (fetch_next_object_name(found)) {
    if (type_name == found) {
      return true;
    }
    set_error(format_to_error(sec::type_clash, "{}: expected type {}, got {}",
                              __func__, type_name, found));
    return false;
  }
  set_error(format_to_error(sec::type_clash, "{}: expected type {}, got none",
                            __func__, type_name));
  return false;
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

bool deserializer::value(strong_actor_ptr& ptr) {
  if (auto* codec = actor_handle_codec()) {
    return codec->load(*this, ptr);
  }
  set_error(make_error(sec::no_actor_handle_codec));
  return false;
}

bool deserializer::value(weak_actor_ptr& ptr) {
  strong_actor_ptr tmp;
  if (!value(tmp)) {
    return false;
  }
  ptr = tmp;
  return true;
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

bool deserializer::value(type_id_list& xs) {
  size_t size = 0;
  if (!begin_sequence(size))
    return false;
  using type_id_int_t = std::underlying_type_t<type_id_t>;
  constexpr size_t max_size = std::numeric_limits<type_id_int_t>::max();
  if (size > max_size) {
    emplace_error(sec::invalid_argument);
    return false;
  }
  if (size == 0) {
    xs = make_type_id_list();
    return end_sequence();
  }
  std::string type_name;
  detail::type_id_list_builder ids{size};
  for (size_t i = 0; i < size; ++i) {
    if (!value(type_name))
      return false;
    auto id = to_type_id(type_name);
    if (id == invalid_type_id) {
      emplace_error(sec::unknown_type, type_name);
      return false;
    }
    ids.push_back(id);
  }
  if (!end_sequence())
    return false;
  xs = ids.move_to_list();
  return true;
}

} // namespace caf
