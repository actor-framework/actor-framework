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

#include "caf/instrumentation/name_registry.hpp"

#include "caf/detail/pretty_type_name.hpp"

namespace caf {
namespace instrumentation {

actortype_id name_registry::get_actortype(const std::type_info& ti) {
  auto hash = ti.hash_code();

  auto it = actortypes_.find(hash);
  if (it == actortypes_.end()) {
    actortypes_[hash] = detail::pretty_type_name(ti);
  }

  return hash;
}

std::string name_registry::identify_actortype(actortype_id cs) const {
  auto it = actortypes_.find(cs);
  return (it != actortypes_.end()) ? it->second : "?";
}

uint64_t name_registry::get_simple_signature(const type_erased_tuple& m) {
  if (m.size() == 0) // NOTE: m.empty() doesn't work here for some reason... (ex: when using dynamically-generated messages)
    return 0;

  auto type = m.type(0);
  size_t hash;
  if (type.first == type_nr<atom_value>::value) {
    hash = static_cast<size_t>(m.get_as<atom_value>(0));
  } else if (type.first != 0) {
    hash = type.first;
  } else {
    hash = type.second->hash_code();
  }

  auto it = signatures_.find(hash);
  if (it == signatures_.end()) {
    if (type.first == type_nr<atom_value>::value)
      signatures_[hash] = "'" + to_string(m.get_as<atom_value>(0)) + "'";
    else if (type.first != 0)
      signatures_[hash] = numbered_type_names[type.first];
    else
      signatures_[hash] = detail::pretty_type_name(*type.second);
  }

  return hash;
}

// TODO remove code duplication, maybe now it would work with a template thanks to the #include "message.hpp"?
uint64_t name_registry::get_simple_signature(const message& m) {
  if (m.size() == 0) // NOTE: m.empty() doesn't work here for some reason... (ex: when using dynamically-generated messages)
    return 0;

  auto type = m.type(0);
  size_t hash;
  if (type.first == type_nr<atom_value>::value) {
    hash = static_cast<size_t>(m.get_as<atom_value>(0));
  } else if (type.first != 0) {
    hash = type.first;
  } else {
    hash = type.second->hash_code();
  }

  auto it = signatures_.find(hash);
  if (it == signatures_.end()) {
    if (type.first == type_nr<atom_value>::value)
      signatures_[hash] = "'" + to_string(m.get_as<atom_value>(0)) + "'";
    else if (type.first != 0)
      signatures_[hash] = numbered_type_names[type.first];
    else
      signatures_[hash] = detail::pretty_type_name(*type.second);
  }

  return hash;
}

std::string name_registry::identify_simple_signature(uint64_t cs) const {
  auto it = signatures_.find(cs);
  return (it != signatures_.end()) ? it->second : "?";
}

} // namespace instrumentation
} // namespace caf
