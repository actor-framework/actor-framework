// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/meta/handler.hpp"

#include "caf/type_id.hpp"

namespace caf::meta {

std::string to_string(const handler& hdl) {
  std::string result;
  result.reserve(64);
  auto render_list = [&result](type_id_list xs) {
    if (xs.empty()) {
      result += "()";
    } else {
      result += '(';
      auto i = xs.begin();
      result += query_type_name(*i++);
      while (i != xs.end()) {
        result += ", ";
        result += query_type_name(*i++);
      }
      result += ')';
    }
  };
  render_list(hdl.inputs);
  result += " -> ";
  render_list(hdl.outputs);
  return result;
}

bool handler_list::contains(const handler& what) const noexcept {
  for (size_t index = 0; index < size; ++index) {
    if (data[index] == what) {
      return true;
    }
  }
  return false;
}

std::string to_string(const handler_list& list) {
  if (list.empty()) {
    return "[]";
  }
  std::string result;
  result.reserve(64);
  result += '[';
  auto i = list.begin();
  result += to_string(*i++);
  while (i != list.end()) {
    result += ", ";
    result += to_string(*i++);
  }
  result += ']';
  return result;
}

bool assignable(const handler_list& lhs, const handler_list& rhs) noexcept {
  // Short-circuit if the assigned-from list is too small.
  if (lhs.size > rhs.size) {
    return false;
  }
  // Short-circuit if lhs and rhs are the same list.
  if (lhs.data == rhs.data) {
    return true;
  }
  // Check if each handler in lhs is present in rhs.
  for (size_t index = 0; index < lhs.size; ++index) {
    if (!rhs.contains(lhs.data[index])) {
      return false;
    }
  }
  return true;
}

} // namespace caf::meta
