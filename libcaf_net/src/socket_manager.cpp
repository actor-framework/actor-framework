// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/socket_manager.hpp"

#include "caf/config.hpp"
#include "caf/net/actor_shell.hpp"
#include "caf/net/multiplexer.hpp"

namespace caf::net {

socket_manager::socket_manager(socket handle, multiplexer* mpx)
  : handle_(handle), mpx_(mpx) {
  CAF_ASSERT(handle_ != invalid_socket);
  CAF_ASSERT(mpx_ != nullptr);
  memset(&flags_, 0, sizeof(flags_t));
}

socket_manager::~socket_manager() {
  close(handle_);
}

actor_system& socket_manager::system() noexcept {
  CAF_ASSERT(mpx_ != nullptr);
  return mpx_->system();
}

void socket_manager::close_read() noexcept {
  // TODO: extend transport API for closing read operations.
  flags_.read_closed = true;
}

void socket_manager::close_write() noexcept {
  // TODO: extend transport API for closing write operations.
  flags_.write_closed = true;
}

void socket_manager::abort_reason(error reason) noexcept {
  abort_reason_ = std::move(reason);
  flags_.read_closed = true;
  flags_.write_closed = true;
}

void socket_manager::register_reading() {
  if (!read_closed())
    mpx_->register_reading(this);
}

void socket_manager::register_writing() {
  if (!write_closed())
    mpx_->register_writing(this);
}

socket_manager_ptr socket_manager::do_handover() {
  flags_.read_closed = true;
  flags_.write_closed = true;
  auto hdl = handle_;
  handle_ = invalid_socket;
  if (auto ptr = make_next_manager(hdl)) {
    return ptr;
  } else {
    close(hdl);
    return nullptr;
  }
}

socket_manager_ptr socket_manager::make_next_manager(socket) {
  return {};
}

} // namespace caf::net
