// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/socket_manager.hpp"

#include "caf/config.hpp"
#include "caf/net/actor_shell.hpp"
#include "caf/net/multiplexer.hpp"

namespace caf::net {

socket_manager::socket_manager(socket handle, multiplexer* parent)
  : handle_(handle), mask_(operation::none), parent_(parent) {
  CAF_ASSERT(handle_ != invalid_socket);
  CAF_ASSERT(parent != nullptr);
}

socket_manager::~socket_manager() {
  close(handle_);
}

actor_system& socket_manager::system() noexcept {
  CAF_ASSERT(parent_ != nullptr);
  return parent_->system();
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
  parent_->register_reading(this);
}

void socket_manager::register_writing() {
  if ((mask() & operation::write) == operation::write)
    return;
  parent_->register_writing(this);
}

} // namespace caf::net
