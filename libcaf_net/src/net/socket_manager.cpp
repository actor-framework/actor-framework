// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/socket_manager.hpp"

#include "caf/config.hpp"
#include "caf/make_counted.hpp"
#include "caf/net/actor_shell.hpp"
#include "caf/net/multiplexer.hpp"

namespace caf::net {

// -- constructors, destructors, and assignment operators ----------------------

socket_manager::socket_manager(multiplexer* mpx, socket fd,
                               event_handler_ptr handler)
  : fd_(fd), mpx_(mpx), handler_(std::move(handler)) {
  CAF_ASSERT(fd_ != invalid_socket);
  CAF_ASSERT(mpx_ != nullptr);
  CAF_ASSERT(handler_ != nullptr);
  memset(&flags_, 0, sizeof(flags_t));
}

socket_manager::~socket_manager() {
  close(fd_);
}

// -- factories ----------------------------------------------------------------

socket_manager_ptr socket_manager::make(multiplexer* mpx, socket handle,
                                        event_handler_ptr handler) {
  return make_counted<socket_manager>(mpx, handle, std::move(handler));
}

// -- properties ---------------------------------------------------------------

actor_system& socket_manager::system() noexcept {
  CAF_ASSERT(mpx_ != nullptr);
  return mpx_->system();
}

// -- event loop management ----------------------------------------------------

void socket_manager::register_reading() {
  mpx_->register_reading(this);
}

void socket_manager::continue_reading() {
  mpx_->continue_reading(this);
}

void socket_manager::register_writing() {
  mpx_->register_writing(this);
}

void socket_manager::continue_writing() {
  mpx_->continue_writing(this);
}

// -- callbacks for the multiplexer --------------------------------------------

void socket_manager::close_read() noexcept {
  // TODO: extend transport API for closing read operations.
  flags_.read_closed = true;
}

void socket_manager::close_write() noexcept {
  // TODO: extend transport API for closing write operations.
  flags_.write_closed = true;
}

socket_manager_ptr socket_manager::do_handover() {
  flags_.read_closed = true;
  flags_.write_closed = true;
  auto fd = fd_;
  fd_ = invalid_socket;
  if (auto ptr = make_next_manager(fd)) {
    return ptr;
  } else {
    close(fd);
    return nullptr;
  }
}

error socket_manager::init(const settings& cfg) {
  CAF_LOG_TRACE(CAF_ARG(cfg));
  if (auto err = nonblocking(fd_, true)) {
    CAF_LOG_ERROR("failed to set nonblocking flag in socket:" << err);
    return err;
  }
  return handler_->init(this, cfg);
}

socket_manager::read_result socket_manager::handle_read_event() {
  auto result = handler_->handle_read_event();
  switch (result) {
    default:
      break;
    case read_result::close:
      flags_.read_closed = true;
      break;
    case read_result::abort:
      flags_.read_closed = true;
      flags_.write_closed = true;
  }
  return result;
}

socket_manager::read_result socket_manager::handle_buffered_data() {
  return handler_->handle_buffered_data();
}

socket_manager::read_result socket_manager::handle_continue_reading() {
  return handler_->handle_continue_reading();
}

socket_manager::write_result socket_manager::handle_write_event() {
  return handler_->handle_write_event();
}

socket_manager::write_result socket_manager::handle_continue_writing() {
  return handler_->handle_continue_writing();
}

void socket_manager::handle_error(sec code) {
  if (handler_) {
    handler_->abort(make_error(code));
    handler_ = nullptr;
  }
}

socket_manager_ptr socket_manager::make_next_manager(socket) {
  return {};
}

} // namespace caf::net
