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

group::group(abstract_group* ptr) : ptr_(ptr) {
  // nop
}

group::group(abstract_group* ptr, bool add_ref) : ptr_(ptr, add_ref) {
  // nop
}

group::group(const invalid_group_t&) : ptr_(nullptr) {
  // nop
}

group::group(abstract_group_ptr gptr) : ptr_(std::move(gptr)) {
  // nop
}

group& group::operator=(const invalid_group_t&) {
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
  if (!origin || origin == sys.node())
    return sys.groups().get(mod, id);
  else if (auto& get_remote = sys.groups().get_remote)
    return get_remote(origin, mod, id);
  else
    return make_error(sec::feature_disabled,
                      "cannot access remote group: middleman not loaded");
}

std::string to_string(const group& x) {
  if (x == invalid_group)
    return "<invalid-group>";
  else
    return x.get()->to_string();
}

} // namespace caf
