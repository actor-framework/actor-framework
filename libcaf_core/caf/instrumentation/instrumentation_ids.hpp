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

#include <typeindex>
#include <utility>

namespace caf {
namespace instrumentation {

using actortype_id = std::type_index;
using msgtype_id = uint64_t; // can be {}, a builtin, an atom or a const typeinfo*
namespace detail {
  using rtti_pair = std::pair<uint16_t, const std::type_info *>;

  msgtype_id get_from_pair(const rtti_pair&);

  template <class T> msgtype_id get(const T&) {
    std::integral_constant<uint16_t, caf::type_nr<T>::value> token;
    const std::type_info* type = (token == 0) ? &typeid(T) : nullptr;
    return get_from_pair(std::make_pair(token, type));
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




std::string to_string(instrumentation::actortype_id actortype);
std::string to_string(instrumentation::msgtype_id msg);

} // namespace instrumentation
} // namespace caf

namespace std {

// stolen from boost::hash_combine
template<class T>
inline void hash_combine(std::size_t &seed, T const &v) {
  seed ^= std::hash<T>()(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template<>
struct hash<std::pair<caf::instrumentation::actortype_id, caf::instrumentation::msgtype_id>> {
  size_t operator()(const std::pair<caf::instrumentation::actortype_id, caf::instrumentation::msgtype_id> &p) const {
    auto hash = p.first.hash_code();
    hash_combine(hash, p.second);
    return hash;
  }
};

} // namespace std

#endif // CAF_INSTRUMENTATION_IDS_HPP
