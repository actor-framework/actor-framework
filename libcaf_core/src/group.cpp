/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#include "caf/group.hpp"

#include "caf/message.hpp"
#include "caf/actor_cast.hpp"
#include "caf/actor_system.hpp"
#include "caf/group_manager.hpp"

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

void serialize(serializer& sink, const group& x, const unsigned int) {
  auto ptr = x.get();
  if (! ptr) {
    std::string dummy;
    sink << dummy;
  } else {
    sink << ptr->module().name();
    ptr->save(sink);
  }
}

void serialize(deserializer& source, group& x, const unsigned int) {
  std::string module_name;
  source >> module_name;
  if (module_name.empty()) {
    x = invalid_group;
    return;
  }
  if (! source.context())
    CAF_RAISE_ERROR("Cannot serialize group without context.");
  auto& sys = source.context()->system();
  auto mod = sys.groups().get_module(module_name);
  if (! mod)
    CAF_RAISE_ERROR("Cannot deserialize a group for unknown module: "
                    + module_name);
  mod->load(source, x);
}

std::string to_string(const group& x) {
  if (x == invalid_group)
    return "<invalid-group>";
  std::string result = x->module().name();
  result += "/";
  result += x->identifier();
  return result;
}

} // namespace caf
