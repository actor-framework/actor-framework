/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/all.hpp"
#include "caf/message.hpp"
#include "caf/abstract_group.hpp"
#include "caf/detail/shared_spinlock.hpp"

#include "caf/detail/group_manager.hpp"
#include "caf/detail/singletons.hpp"

namespace caf {

abstract_group::subscription::subscription(const abstract_group_ptr& g)
    : m_group(g) {
  // nop
}

void abstract_group::subscription::actor_exited(abstract_actor* ptr, uint32_t) {
  m_group->unsubscribe(ptr->address());
}

bool abstract_group::subscription::matches(const token& what) {
  if (what.subtype != typeid(subscription_token)) {
    return false;
  }
  auto& ot = *reinterpret_cast<const subscription_token*>(what.ptr);
  return ot.group == m_group;
}

abstract_group::module::module(std::string name) : m_name(std::move(name)) {
  // nop
}

const std::string& abstract_group::module::name() {
  return m_name;
}

abstract_group::abstract_group(abstract_group::module_ptr mod, std::string id)
    : m_module(mod), m_identifier(std::move(id)) {
  // nop
}

const std::string& abstract_group::identifier() const {
  return m_identifier;
}

abstract_group::module_ptr abstract_group::get_module() const {
  return m_module;
}

const std::string& abstract_group::module_name() const {
  return get_module()->name();
}

abstract_group::module::~module() {
  // nop
}

abstract_group::~abstract_group() {
  // nop
}

} // namespace caf
