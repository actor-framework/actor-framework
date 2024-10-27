// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/serializer.hpp"

#include "caf/actor_system.hpp"

namespace caf {

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

bool serializer::value(const strong_actor_ptr& ptr) {
  auto aid = actor_id{0};
  auto nid = node_id{};
  if (ptr != nullptr) {
    aid = ptr->id();
    nid = ptr->node();
  }
  auto ok = object(ptr).pretty_name("actor").fields(field("id", aid),
                                                    field("node", nid));
  if (!ok) {
    return false;
  }
  if (ptr != nullptr) {
    if (auto err = save_actor(ptr, aid, nid)) {
      set_error(err);
      return false;
    }
  }
  return true;
}

bool serializer::value(const weak_actor_ptr& ptr) {
  auto tmp = ptr.lock();
  return value(tmp);
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
