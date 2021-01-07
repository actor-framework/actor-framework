// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

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
