/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/net/socket_manager.hpp"

#include "caf/config.hpp"
#include "caf/net/multiplexer.hpp"

namespace caf::net {

socket_manager::socket_manager(socket handle, const multiplexer_ptr& parent)
  : handle_(handle), mask_(operation::none), parent_(parent) {
  CAF_ASSERT(parent != nullptr);
  CAF_ASSERT(handle_ != invalid_socket);
}

socket_manager::~socket_manager() {
  close(handle_);
}

bool socket_manager::mask_add(operation flag) noexcept {
  CAF_ASSERT(flag != operation::none);
  auto x = mask();
  if ((x & flag) == flag)
    return false;
  mask_ = x | flag;
  return true;
}

bool socket_manager::mask_del(operation flag) noexcept {
  CAF_ASSERT(flag != operation::none);
  auto x = mask();
  if ((x & flag) == operation::none)
    return false;
  mask_ = x & ~flag;
  return true;
}

void socket_manager::register_reading() {
  if ((mask() & operation::read) == operation::read)
    return;
  auto ptr = parent_.lock();
  if (ptr != nullptr)
    ptr->register_reading(this);
}

void socket_manager::register_writing() {
  if ((mask() & operation::write) == operation::write)
    return;
  auto ptr = parent_.lock();
  if (ptr != nullptr)
    ptr->register_writing(this);
}

} // namespace caf::net
