// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include <cstdint>
#include <string_view>

namespace caf::internal {

/// Reflects the structure of JSON objects according to ECMA-404.
enum class json_node : uint8_t {
  element, /// Can morph into any other type except `member`.
  object,  /// Contains any number of members.
  member,  /// A single key-value pair.
  key,     /// The key of a field.
  array,   /// Contains any number of elements.
  string,  /// A character sequence (terminal type).
  number,  /// An integer or floating point (terminal type).
  boolean, /// Either "true" or "false" (terminal type).
  null,    /// The literal "null" (terminal type).
};

/// @relates json_node
constexpr bool can_morph(json_node from, json_node to) {
  return from == json_node::element && to != json_node::member;
}

/// @relates json_node
constexpr std::string_view as_json_type_name(json_node tag) {
  switch (tag) {
    case json_node::element:
      return "element";
    case json_node::object:
      return "object";
    case json_node::member:
      return "member";
    case json_node::array:
      return "array";
    case json_node::string:
      return "string";
    case json_node::number:
      return "number";
    case json_node::boolean:
      return "bool";
    default:
      return "null";
  }
}

} // namespace caf::internal
