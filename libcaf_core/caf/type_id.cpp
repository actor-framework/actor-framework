// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/type_id.hpp"

#include "caf/detail/meta_object.hpp"

namespace caf {

std::string_view query_type_name(type_id_t type) {
  if (auto meta = detail::global_meta_object_or_null(type))
    return meta->type_name;
  return {};
}

type_id_t query_type_id(std::string_view name) {
  auto objects = detail::global_meta_objects();
  for (size_t index = 0; index < objects.size(); ++index)
    if (objects[index].type_name == name)
      return static_cast<type_id_t>(index);
  return invalid_type_id;
}

bool is_system_message(type_id_t type) noexcept {
  switch (type) {
    case type_id_v<exit_msg>:
    case type_id_v<down_msg>:
    case type_id_v<error>:
      return true;
    default:
      return false;
  }
}

type_id_mapper::~type_id_mapper() {
  // nop
}

std::string_view default_type_id_mapper::operator()(type_id_t type) const {
  return query_type_name(type);
}

type_id_t default_type_id_mapper::operator()(std::string_view name) const {
  return query_type_id(name);
}

} // namespace caf
