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

#ifndef CAF_INSTRUMENTATION_IDS_HPP
#define CAF_INSTRUMENTATION_IDS_HPP

#include "caf/type_erased_tuple.hpp"
#include "caf/message.hpp"

#include <unordered_map>
#include <typeindex>
#include <utility>

namespace caf {
namespace instrumentation {

using actortype_id = std::type_index;
using msgtype_id = uint64_t; // can be {}, a builtin, an atom or a const typeinfo*

struct instrumented_actor_id {
  actortype_id type;
  actor_id id;
  bool operator==(const instrumented_actor_id&) const noexcept;
};
struct sender {
  instrumented_actor_id actor;
  msgtype_id message;
  bool operator==(const sender&) const noexcept;
};
struct aggregate_sender {
  actortype_id actor_type;
  msgtype_id message;
  bool operator==(const aggregate_sender&) const noexcept;
};

namespace detail {
  using rtti_pair = std::pair<uint16_t, const std::type_info*>;

  msgtype_id get_from_pair(const rtti_pair&);

  msgtype_id get(const atom_value& atom);

  template <class T> msgtype_id get(const T&) {
    uint16_t token = caf::type_nr<T>::value;
    const std::type_info* type = (token == 0) ? &typeid(T) : nullptr;
    return get_from_pair(std::make_pair(token, type));
  }

  template <atom_value V> msgtype_id get(const atom_constant<V>&) {
      return get(V);
  }
}

msgtype_id get_msgtype();

template <class tuple_type, typename std::enable_if<
            std::is_base_of<caf::message, tuple_type>{} ||
            std::is_base_of<caf::type_erased_tuple, tuple_type>{},
            int>::type = 0>
msgtype_id get_msgtype(const tuple_type& t) {
  if (t.size() == 0)
    return get_msgtype();

  auto type = t.type(0);
  if (type.first == type_nr<atom_value>::value)
    return detail::get(t.template get_as<atom_value>(0));

  return detail::get_from_pair(type);
}

template <class T, class...Ts> msgtype_id get_msgtype(const T& first, const Ts&...) {
  return detail::get(first);
}

instrumented_actor_id get_instrumented_actor_id(const abstract_actor& actor);

std::string to_string(actortype_id actortype);
std::string to_string(msgtype_id msg);

template<typename K, typename V>
static void combine_map(std::unordered_map<K, V>& dst, const std::unordered_map<K, V>& src) {
  for (const auto& it : src) {
    dst[it.first].combine(it.second);
  }
}
template<typename K, typename V>
static void sum_map(std::unordered_map<K, V>& dst, const std::unordered_map<K, V>& src) {
    for (const auto& it : src) {
        dst[it.first] += it.second;
    }
}

} // namespace instrumentation
} // namespace caf

namespace std {
template<>
struct hash<caf::instrumentation::instrumented_actor_id> {
  std::size_t operator()(const caf::instrumentation::instrumented_actor_id&) const noexcept;
};
template <>
struct hash<caf::instrumentation::sender> {
  std::size_t operator()(const caf::instrumentation::sender&) const noexcept;
};

template <>
struct hash<caf::instrumentation::aggregate_sender> {
    std::size_t operator()(const caf::instrumentation::aggregate_sender&) const noexcept;
};
} // namespace std

#endif // CAF_INSTRUMENTATION_IDS_HPP
