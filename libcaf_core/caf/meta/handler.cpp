// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/meta/handler.hpp"

#include "caf/type_id.hpp"

namespace caf::meta {

std::string to_string(handler hdl) {
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

} // namespace caf::meta
