// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include <cstdint>
#include <string_view>

namespace caf::internal {

enum class stringification_inspector_node : uint8_t {
  sequence, /// Can morph into any other type except `member`.
  map,      /// A map-like structure, e.g., a dictionary or associative array.
  object,   /// Contains any number of members.
  member,   /// A single key-value pair.
  null,     /// A null value.
};

/// @relates stringification_inspector_node
constexpr std::string_view
as_stringification_type_name(stringification_inspector_node tag) {
  switch (tag) {
    case stringification_inspector_node::sequence:
      return "sequence";
    case stringification_inspector_node::map:
      return "map";
    case stringification_inspector_node::object:
      return "object";
    case stringification_inspector_node::member:
      return "member";
    default:
      return "null";
  }
}

} // namespace caf::internal
