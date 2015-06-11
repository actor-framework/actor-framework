/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#ifndef CAF_UNIFORM_TYPE_INFO_MAP_HPP
#define CAF_UNIFORM_TYPE_INFO_MAP_HPP

#include <set>
#include <map>
#include <string>
#include <utility>
#include <type_traits>

#include "caf/fwd.hpp"

#include "caf/atom.hpp"
#include "caf/unit.hpp"
#include "caf/node_id.hpp"
#include "caf/duration.hpp"
#include "caf/system_messages.hpp"

#include "caf/detail/type_list.hpp"

#include "caf/detail/singleton_mixin.hpp"

namespace caf {
class uniform_type_info;
} // namespace caf

namespace caf {
namespace detail {

// lookup table for integer types
extern const char* mapped_int_names[][2];

template <class T>
constexpr const char* mapped_int_name() {
  return mapped_int_names[sizeof(T)][std::is_signed<T>::value ? 1 : 0];
}

class uniform_type_info_map_helper;

// note: this class is implemented in uniform_type_info.cpp
class uniform_type_info_map {

  friend class uniform_type_info_map_helper;
  friend class singleton_mixin<uniform_type_info_map>;

public:

  using pointer = const uniform_type_info*;

  virtual ~uniform_type_info_map();

  virtual pointer by_uniform_name(const std::string& name) = 0;

  virtual pointer by_type_nr(uint16_t) const = 0;

  virtual pointer by_rtti(const std::type_info& ti) const = 0;

  virtual std::vector<pointer> get_all() const = 0;

  virtual pointer insert(const std::type_info*, uniform_type_info_ptr) = 0;

  static uniform_type_info_map* create_singleton();

  inline void dispose() { delete this; }

  void stop();

  virtual void initialize() = 0;

};

} // namespace detail
} // namespace caf

#endif // CAF_UNIFORM_TYPE_INFO_MAP_HPP
