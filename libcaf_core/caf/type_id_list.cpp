// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/type_id_list.hpp"

#include "caf/detail/meta_object.hpp"
#include "caf/detail/type_id_list_builder.hpp"
#include "caf/message.hpp"

#include <algorithm>
#include <numeric>

namespace caf {

size_t type_id_list::data_size() const noexcept {
  return std::accumulate(
    begin(), end(), size_t{0}, [](size_t acc, type_id_t type) {
      return acc + detail::global_meta_object(type).padded_size;
    });
}

std::string to_string(type_id_list xs) {
  if (!xs || xs.size() == 0)
    return "[]";
  std::string result;
  result += '[';
  {
    auto tn = detail::global_meta_object(xs[0]).type_name;
    result.insert(result.end(), tn.begin(), tn.end());
  }
  for (size_t index = 1; index < xs.size(); ++index) {
    result += ", ";
    auto tn = detail::global_meta_object(xs[index]).type_name;
    result.insert(result.end(), tn.begin(), tn.end());
  }
  result += ']';
  return result;
}

type_id_list type_id_list::concat(std::span<type_id_list> lists) {
  auto total_size = std::accumulate(lists.begin(), lists.end(), size_t{0},
                                    [](size_t acc, type_id_list ls) {
                                      return acc + ls.size();
                                    });
  detail::type_id_list_builder builder{total_size};
  std::ranges::for_each(lists, [&](type_id_list ls) {
    std::ranges::for_each(ls, [&](type_id_t id) { builder.push_back(id); });
  });
  return builder.move_to_list();
}

type_id_list types_of(const message& msg) {
  return msg.types();
}

} // namespace caf
