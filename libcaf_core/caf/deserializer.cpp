// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/deserializer.hpp"

#include "caf/actor_system.hpp"
#include "caf/format_to_error.hpp"

namespace caf {

deserializer::~deserializer() {
  // nop
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
    err_ = format_to_error(sec::type_clash, "{}: expected type {}, got {}",
                           __func__, type_name, found);
    return false;
  }
  err_ = format_to_error(sec::type_clash, "{}: expected type {}, got none",
                         __func__, type_name);
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
  auto aid = actor_id{0};
  auto nid = node_id{};
  auto ok = object(ptr).pretty_name("actor").fields(field("id", aid),
                                                    field("node", nid));
  if (!ok) {
    return false;
  }
  if (aid == 0 || !nid) {
    ptr = nullptr;
  } else if (auto err = load_actor(ptr, context_, aid, nid)) {
    set_error(err);
    return false;
  }
  return true;
}

bool deserializer::value(weak_actor_ptr& ptr) {
  strong_actor_ptr tmp;
  if (!value(tmp)) {
    return false;
  }
  ptr.reset(tmp.get());
  return true;
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
