// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/group_module.hpp"

namespace caf {

group_module::group_module(actor_system& sys, std::string mname)
  : system_(&sys), name_(std::move(mname)) {
  // nop
}

group_module::~group_module() {
  // nop
}

} // namespace caf
