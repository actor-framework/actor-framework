// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/type_id_list.hpp"

#include "caf/detail/meta_object.hpp"
#include "caf/detail/type_id_list_builder.hpp"
#include "caf/message.hpp"

namespace caf {

size_t type_id_list::data_size() const noexcept {
  auto result = size_t{0};
  for (auto type : *this) {
    auto meta = detail::global_meta_object(type);
    result += meta->padded_size;
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

type_id_list type_id_list::concat(span<type_id_list> lists) {
  auto total_size = size_t{0};
  for (auto ls : lists)
    total_size += ls.size();
  detail::type_id_list_builder builder;
  builder.reserve(total_size);
  for (auto ls : lists)
    for (auto id : ls)
      builder.push_back(id);
  return builder.move_to_list();
}

type_id_list types_of(const message& msg) {
  return msg.types();
}

} // namespace caf
