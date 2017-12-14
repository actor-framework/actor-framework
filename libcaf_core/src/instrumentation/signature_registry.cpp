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

#include <string>
#include <cstdint>
#include <unordered_map>

#include "caf/detail/pretty_type_name.hpp"
#include "caf/instrumentation/signature_registry.hpp"

// stolen from boost::hash_combine
template <class T>
inline void hash_combine(std::size_t& seed, T const& v) {
  seed ^= std::hash<T>()(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
}

namespace caf {
namespace instrumentation {

actortype_id signature_registry::get_actortype(const std::type_info& ti) {
  auto hash = ti.hash_code();

  auto it = actortypes_.find(hash);
  if (it == actortypes_.end()) {
    actortypes_[hash] = detail::pretty_type_name(ti);
  }

  return hash;
}

std::string signature_registry::identify_actortype(actortype_id cs) const {
  auto it = actortypes_.find(cs);
  return (it != actortypes_.end()) ? it->second : "?";
}

uint64_t signature_registry::get_signature(const type_erased_tuple &m) {
  if (m.size() == 0) // NOTE: m.empty() doesn't work here for some reason... (ex: when using dynamically-generated messages)
    return 0;

  size_t hash = 0;
  for (size_t idx = 0; idx < m.size(); ++idx) {
    auto type = m.type(idx);
    if (type.first != 0)
      hash_combine(hash, type.first);
    else
      hash_combine(hash, type.second);
  }

  auto it = signatures_.find(hash);
  if (it == signatures_.end()) {
    std::string sig;
    for (size_t idx = 0; idx < m.size(); ++idx) {
      auto type = m.type(idx);
      if (type.first == type_nr<atom_value>::value)
        sig += "'" + to_string(m.get_as<atom_value>(idx)) + "'";
      else if (type.first != 0)
        sig += numbered_type_names[type.first];
      else
        sig += detail::pretty_type_name(*type.second);

      if (idx < m.size() - 1) {
        sig += ", ";
      }
    }
    signatures_[hash] = sig;
  }

  return hash;
}

std::string signature_registry::identify_signature(uint64_t cs) const {
  auto it = signatures_.find(cs);
  return (it != signatures_.end()) ? it->second : "?";
}

} // namespace instrumentation
} // namespace caf
