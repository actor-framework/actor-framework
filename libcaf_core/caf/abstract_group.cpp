// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/abstract_group.hpp"

#include "caf/actor_cast.hpp"
#include "caf/group.hpp"
#include "caf/group_manager.hpp"
#include "caf/group_module.hpp"
#include "caf/message.hpp"

namespace caf {

abstract_group::abstract_group(group_module_ptr mod, std::string id,
                               node_id nid)
  : abstract_channel(abstract_channel::is_abstract_group_flag),
    parent_(std::move(mod)),
    origin_(std::move(nid)),
    identifier_(std::move(id)) {
  // nop
}

abstract_group::~abstract_group() {
  // nop
}

actor_system& abstract_group::system() const noexcept {
  return module().system();
}

std::string abstract_group::stringify() const {
  auto result = module().name();
  result += ':';
  result += identifier();
  return result;
}

actor abstract_group::intermediary() const noexcept {
  return {};
}

} // namespace caf
