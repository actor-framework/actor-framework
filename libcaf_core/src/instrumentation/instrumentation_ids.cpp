/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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

#include "caf/instrumentation/instrumentation_ids.hpp"

#include "caf/detail/pretty_type_name.hpp"
#include "caf/string_algorithms.hpp"
#include "caf/abstract_actor.hpp"
#include "caf/detail/hash.hpp"
#include "caf/type_nr.hpp"

#include <typeindex>
#include <cstdint>
#include <utility>

namespace caf {
namespace instrumentation {

bool msgtype_id::operator==(const msgtype_id& other) const noexcept {
  return type == other.type && value.value == other.value.value;
}

namespace detail {

  msgtype_id get_from_pair(const rtti_pair& pair) {
    msgtype_id::value_t v;
    if (pair.first != 0) {
      v.builtin = pair.first;
      return { msgtype_id::BUILTIN, v };
    } else {
      v.typeinfo = pair.second;
      return { msgtype_id::TYPEINFO, v };
    }
  }

  msgtype_id get(const atom_value& atom) {
    msgtype_id::value_t v;
    v.atom = atom;
    return { msgtype_id::ATOM, v };
  }

} // namespace detail

bool instrumented_actor_id::operator==(const instrumented_actor_id& other) const noexcept {
  return type == other.type && id == other.id;
}

bool sender::operator==(const sender& other) const noexcept {
  return actor == other.actor && message == other.message;
}

bool aggregate_sender::operator==(const aggregate_sender& other) const noexcept {
  return actor_type == other.actor_type && message == other.message;
}

msgtype_id get_msgtype() {
  msgtype_id::value_t v;
  v.value = 0;
  return { msgtype_id::EMPTY, v };
}

std::string to_string(msgtype_id msg) {
  switch (msg.type) {
    case msgtype_id::EMPTY:
      return "{}";
    case msgtype_id::BUILTIN:
      return numbered_type_names[msg.value.builtin - 1];
    case msgtype_id::ATOM:
      return to_string(msg.value.atom);
    case msgtype_id::TYPEINFO:
      return caf::detail::pretty_type_name(*msg.value.typeinfo);
    default:
      return "?"; // TODO
  }
}

instrumented_actor_id get_instrumented_actor_id(const abstract_actor& actor) {
  actortype_id actortype = typeid(actor);
  actor_id actorid = actor.id();
  return {actortype, actorid};
}

std::string to_string(actor_id actorid) {
  return std::to_string(actorid);
}

std::string to_string(actortype_id actortype) {
  return caf::detail::pretty_type_name(actortype);
}

} // namespace instrumentation
} // namespace caf

namespace std
{

using namespace caf::instrumentation;

size_t hash<msgtype_id>::operator()(const msgtype_id& v) const noexcept {
  size_t seed = 0;
  caf::detail::hash_combine(seed, v.type);
  caf::detail::hash_combine(seed, v.value.value);
  return seed;
}

size_t hash<instrumented_actor_id>::operator()(const instrumented_actor_id& v) const noexcept {
  size_t seed = 0;
  caf::detail::hash_combine(seed, v.type);
  caf::detail::hash_combine(seed, v.id);
  return seed;
}

size_t hash<sender>::operator()(const sender& v) const noexcept {
  size_t seed = 0;
  caf::detail::hash_combine(seed, v.actor);
  caf::detail::hash_combine(seed, v.message);
  return seed;
}

size_t hash<aggregate_sender>::operator()(const aggregate_sender& v) const noexcept {
  size_t seed = 0;
  caf::detail::hash_combine(seed, v.actor_type);
  caf::detail::hash_combine(seed, v.message);
  return seed;
}

} // namespace std