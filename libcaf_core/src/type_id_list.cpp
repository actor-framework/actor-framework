// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/type_id_list.hpp"

#include "caf/detail/meta_object.hpp"

namespace caf {

size_t type_id_list::data_size() const noexcept {
  auto result = size_t{0};
  for (auto type : *this) {
    auto meta_obj = detail::global_meta_object(type);
    result += meta_obj->padded_size;
  }
  return result;
}

std::string to_string(type_id_list xs) {
  if (!xs || xs.size() == 0)
    return "[]";
  std::string result;
  result += '[';
  {
    auto tn = detail::global_meta_object(xs[0])->type_name;
    result.insert(result.end(), tn.begin(), tn.end());
  }
  for (size_t index = 1; index < xs.size(); ++index) {
    result += ", ";
    auto tn = detail::global_meta_object(xs[index])->type_name;
    result.insert(result.end(), tn.begin(), tn.end());
  }
  result += ']';
  return result;
}

} // namespace caf
