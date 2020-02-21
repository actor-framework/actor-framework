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

namespace {

template <class Serializer>
auto save_group(Serializer& sink, group& x) {
  std::string mod_name;
  auto ptr = x.get();
  if (ptr == nullptr)
    return sink(mod_name);
  mod_name = ptr->module().name();
  if (auto err = sink(mod_name))
    return err;
  return ptr->save(sink);
}

} // namespace

error inspect(serializer& sink, group& x) {
  return save_group(sink, x);
}

error_code<sec> inspect(binary_serializer& sink, group& x) {
  return save_group(sink, x);
}

namespace {

template <class Deserializer>
typename Deserializer::result_type load_group(Deserializer& source, group& x) {
  std::string module_name;
  if (auto err = source(module_name))
    return err;
  if (module_name.empty()) {
    x = invalid_group;
    return none;
  }
  if (source.context() == nullptr)
    return sec::no_context;
  auto& sys = source.context()->system();
  auto mod = sys.groups().get_module(module_name);
  if (!mod)
    return sec::no_such_group_module;
  return mod->load(source, x);
}

} // namespace

error inspect(deserializer& source, group& x) {
  return load_group(source, x);
}

error_code<sec> inspect(binary_deserializer& source, group& x) {
  return load_group(source, x);
}

std::string to_string(const group& x) {
  if (x == invalid_group)
    return "<invalid-group>";
  std::string result = x.get()->module().name();
  result += ":";
  result += x.get()->identifier();
  return result;
}

} // namespace caf
