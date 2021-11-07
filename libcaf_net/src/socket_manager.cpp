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

bool socket_manager::set_read_flag() noexcept {
  auto old = mask_;
  mask_ = add_read_flag(mask_);
  return old != mask_;
}

bool socket_manager::set_write_flag() noexcept {
  auto old = mask_;
  mask_ = add_write_flag(mask_);
  return old != mask_;
}

bool socket_manager::unset_read_flag() noexcept {
  auto old = mask_;
  mask_ = remove_read_flag(mask_);
  return old != mask_;
}

bool socket_manager::unset_write_flag() noexcept {
  auto old = mask_;
  mask_ = remove_write_flag(mask_);
  return old != mask_;
}

void socket_manager::block_reads() noexcept {
  mask_ = net::block_reads(mask_);
}

void socket_manager::block_writes() noexcept {
  mask_ = net::block_writes(mask_);
}

void socket_manager::block_reads_and_writes() noexcept {
  mask_ = operation::shutdown;
}

void socket_manager::register_reading() {
  if (!net::is_reading(mask_) && !is_read_blocked(mask_))
    parent_->register_reading(this);
}

void socket_manager::register_writing() {
  if (!net::is_writing(mask_) && !is_write_blocked(mask_))
    parent_->register_writing(this);
}

void socket_manager::shutdown_reading() {
  parent_->shutdown_reading(this);
}

void socket_manager::shutdown_writing() {
  parent_->shutdown_writing(this);
}

} // namespace caf::net
