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
#include "caf/channel.hpp"
#include "caf/message.hpp"

#include "caf/detail/singletons.hpp"
#include "caf/detail/group_manager.hpp"

namespace caf {

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

intptr_t group::compare(const group& other) const {
  return channel::compare(ptr_.get(), other.ptr_.get());
}

group group::get(const std::string& arg0, const std::string& arg1) {
  return detail::singletons::get_group_manager()->get(arg0, arg1);
}

group group::anonymous() {
  return detail::singletons::get_group_manager()->anonymous();
}

void group::add_module(abstract_group::unique_module_ptr ptr) {
  detail::singletons::get_group_manager()->add_module(std::move(ptr));
}

abstract_group::module_ptr group::get_module(const std::string& module_name) {
  return detail::singletons::get_group_manager()->get_module(module_name);
}

} // namespace caf
