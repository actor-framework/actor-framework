/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/type_id.hpp"

#include "caf/detail/meta_object.hpp"

namespace caf {

string_view query_type_name(type_id_t type) {
  if (auto ptr = detail::global_meta_object(type))
    return ptr->type_name;
  return {};
}

type_id_t query_type_id(string_view name) {
  auto objects = detail::global_meta_objects();
  for (size_t index = 0; index < objects.size(); ++index)
    if (objects[index].type_name == name)
      return static_cast<type_id_t>(index);
  return invalid_type_id;
}

} // namespace caf
