// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/json_object.hpp"

namespace caf::detail {
namespace {

const auto empty_object_instance = json::object{};

} // namespace
} // namespace caf::detail

namespace caf {

// -- constructors, destructors, and assignment operators ----------------------

json_object::json_object() noexcept : obj_(&detail::empty_object_instance) {
  // nop
}

// -- properties ---------------------------------------------------------------

json_value json_object::value(std::string_view key) const {
  auto pred = [key](const auto& member) { return member.key == key; };
  auto i = std::find_if(obj_->begin(), obj_->end(), pred);
  if (i != obj_->end()) {
    return {i->val, storage_};
  }
  return json_value::undefined();
}

std::string to_string(const json_object& obj) {
  std::string result;
  obj.print_to(result);
  return result;
}

} // namespace caf
