// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/group.hpp"

#include "caf/actor_cast.hpp"
#include "caf/actor_system.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/deserializer.hpp"
#include "caf/group_manager.hpp"
#include "caf/message.hpp"
#include "caf/serializer.hpp"

namespace caf {

group::group(invalid_group_t) : ptr_(nullptr) {
  // nop
}

group::group(abstract_group_ptr gptr) : ptr_(std::move(gptr)) {
  // nop
}

group& group::operator=(invalid_group_t) {
  ptr_.reset();
  return *this;
}

intptr_t group::compare(const abstract_group* lhs, const abstract_group* rhs) {
  return reinterpret_cast<intptr_t>(lhs) - reinterpret_cast<intptr_t>(rhs);
}

intptr_t group::compare(const group& other) const noexcept {
  return compare(ptr_.get(), other.ptr_.get());
}

expected<group> group::load_impl(actor_system& sys, const node_id& origin,
                                 const std::string& mod,
                                 const std::string& id) {
  if (!origin || origin == sys.node()) {
    if (mod == "remote") {
      // Local groups on this node appear as remote groups on other nodes.
      // Hence, serializing back and forth results in receiving a "remote"
      // representation for a group that actually runs locally.
      return sys.groups().get_local(id);
    } else {
      return sys.groups().get(mod, id);
    }
  } else if (auto& get_remote = sys.groups().get_remote) {
    return get_remote(origin, mod, id);
  } else {
    return make_error(sec::feature_disabled,
                      "cannot access remote group: middleman not loaded");
  }
}

std::string to_string(const group& x) {
  if (x)
    return x.get()->stringify();
  else
    return "<invalid-group>";
}

} // namespace caf
