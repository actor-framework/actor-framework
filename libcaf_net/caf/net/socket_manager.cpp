// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/socket_manager.hpp"

#include "caf/net/actor_shell.hpp"
#include "caf/net/multiplexer.hpp"

#include "caf/config.hpp"
#include "caf/log/net.hpp"
#include "caf/make_counted.hpp"

namespace caf::net {

// -- constructors, destructors, and assignment operators ----------------------

socket_manager::socket_manager(multiplexer* mpx, event_handler_ptr handler)
  : fd_(handler->handle()),
    mpx_(mpx),
    handler_(std::move(handler)),
    disposed_(false) {
  CAF_ASSERT(fd_ != invalid_socket);
  CAF_ASSERT(mpx_ != nullptr);
  CAF_ASSERT(handler_ != nullptr);
}

socket_manager::~socket_manager() {
  // Note: may not call cleanup since it calls the multiplexer via deregister().
  handler_.reset();
  if (fd_) {
    close(fd_);
    fd_ = invalid_socket;
  }
  if (!cleanup_listeners_.empty()) {
    for (auto& f : cleanup_listeners_)
      mpx_->schedule(std::move(f));
    cleanup_listeners_.clear();
  }
}

// -- factories ----------------------------------------------------------------

socket_manager_ptr socket_manager::make(multiplexer* mpx,
                                        event_handler_ptr handler) {
  CAF_ASSERT(mpx != nullptr);
  return make_counted<socket_manager>(std::move(mpx), std::move(handler));
}

// -- properties ---------------------------------------------------------------

actor_system& socket_manager::system() noexcept {
  CAF_ASSERT(mpx_ != nullptr);
  return mpx_->system();
}

bool socket_manager::is_reading() const noexcept {
  return mpx_->is_reading(this);
}

bool socket_manager::is_writing() const noexcept {
  return mpx_->is_writing(this);
}

// -- event loop management ----------------------------------------------------

void socket_manager::register_reading() {
  mpx_->register_reading(this);
}

void socket_manager::register_writing() {
  mpx_->register_writing(this);
}

void socket_manager::deregister_reading() {
  mpx_->deregister_reading(this);
}

void socket_manager::deregister_writing() {
  mpx_->deregister_writing(this);
}

void socket_manager::deregister() {
  mpx_->deregister(this);
}

// -- callbacks for the handler ------------------------------------------------

void socket_manager::schedule_handover() {
  deregister();
  mpx_->schedule_fn([ptr = strong_this()] {
    event_handler_ptr next;
    if (ptr->handler_->do_handover(next)) {
      ptr->handler_.swap(next);
    }
  });
}

void socket_manager::schedule(action what) {
  // Wrap the action to make sure the socket manager is still alive when running
  // the action later.
  mpx_->schedule_fn([ptr = strong_this(), f = std::move(what)]() mutable {
    CAF_IGNORE_UNUSED(ptr);
    f.run();
  });
}

void socket_manager::shutdown() {
  CAF_LOG_TRACE("");
  if (!shutting_down_) {
    shutting_down_ = true;
    dispose();
  } else {
    // This usually only happens after disposing the manager if the handler
    // still had data to send.
    mpx_->schedule_fn([ptr = strong_this()] { //
      ptr->cleanup();
    });
  }
}

// -- callbacks for the multiplexer --------------------------------------------

error socket_manager::start() {
  CAF_LOG_TRACE("");
  if (auto err = nonblocking(fd_, true)) {
    log::net::error("failed to set nonblocking flag in socket: {}", err);
    handler_->abort(err);
    cleanup();
    return err;
  } else if (err = handler_->start(this); err) {
    log::net::debug("failed to initialize handler: {}", err);
    cleanup();
    return err;
  } else {
    return none;
  }
}

void socket_manager::handle_read_event() {
  if (handler_)
    handler_->handle_read_event();
  else
    deregister();
}

void socket_manager::handle_write_event() {
  if (handler_)
    handler_->handle_write_event();
  else
    deregister();
}

void socket_manager::handle_error(sec code) {
  CAF_LOG_TRACE("");
  if (!disposed_)
    disposed_ = true;
  if (handler_) {
    if (!shutting_down_) {
      handler_->abort(make_error(code));
      shutting_down_ = true;
    }
    if (code == sec::disposed && !handler_->finalized()) {
      // When disposing the manger, the transport is still allowed to send any
      // pending data and it will call shutdown() later to trigger cleanup().
      deregister_reading();
    } else {
      cleanup();
    }
  }
}

// -- implementation of disposable_impl ----------------------------------------

void socket_manager::dispose() {
  bool expected = false;
  if (disposed_.compare_exchange_strong(expected, true)) {
    mpx_->schedule_fn([ptr = strong_this()] { //
      ptr->handle_error(sec::disposed);
    });
  }
}

bool socket_manager::disposed() const noexcept {
  return disposed_.load();
}

void socket_manager::ref_disposable() const noexcept {
  ref();
}

void socket_manager::deref_disposable() const noexcept {
  deref();
}

// -- utility functions --------------------------------------------------------

void socket_manager::cleanup() {
  deregister();
  handler_.reset();
  if (fd_) {
    close(fd_);
    fd_ = invalid_socket;
  }
  if (!cleanup_listeners_.empty()) {
    for (auto& f : cleanup_listeners_)
      mpx_->schedule(std::move(f));
    cleanup_listeners_.clear();
  }
}

socket_manager_ptr socket_manager::strong_this() {
  return socket_manager_ptr{this};
}

} // namespace caf::net
