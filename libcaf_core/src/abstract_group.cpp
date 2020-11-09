/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/abstract_group.hpp"

#include "caf/actor_cast.hpp"
#include "caf/detail/shared_spinlock.hpp"
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
