// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/pollset_updater.hpp"

#include "caf/net/multiplexer.hpp"

#include "caf/action.hpp"
#include "caf/actor_system.hpp"
#include "caf/logger.hpp"
#include "caf/sec.hpp"
#include "caf/span.hpp"

#include <cstring>

namespace caf::detail {

// -- constructors, destructors, and assignment operators ----------------------

pollset_updater::pollset_updater(net::pipe_socket fd) : fd_(fd) {
  // nop
}

// -- factories ----------------------------------------------------------------

std::unique_ptr<pollset_updater> pollset_updater::make(net::pipe_socket fd) {
  return std::make_unique<pollset_updater>(fd);
}

// -- interface functions ------------------------------------------------------

error pollset_updater::start(net::socket_manager* owner) {
  CAF_LOG_TRACE("");
  owner_ = owner;
  mpx_ = owner->mpx_ptr();
  return net::nonblocking(fd_, true);
}

net::socket pollset_updater::handle() const {
  return fd_;
}

void pollset_updater::handle_read_event() {
  CAF_LOG_TRACE("");
  auto as_mgr = [](intptr_t ptr) {
    return intrusive_ptr{reinterpret_cast<net::socket_manager*>(ptr), false};
  };
  auto add_action = [this](intptr_t ptr) {
    auto f = action{intrusive_ptr{reinterpret_cast<action::impl*>(ptr), false}};
    mpx_->pending_actions_.push_back(std::move(f));
  };
  for (;;) {
    CAF_ASSERT((buf_.size() - buf_size_) > 0);
    auto num_bytes
      = read(fd_, make_span(buf_.data() + buf_size_, buf_.size() - buf_size_));
    if (num_bytes > 0) {
      buf_size_ += static_cast<size_t>(num_bytes);
      if (buf_.size() == buf_size_) {
        buf_size_ = 0;
        auto opcode = static_cast<uint8_t>(buf_[0]);
        intptr_t ptr;
        memcpy(&ptr, buf_.data() + 1, sizeof(intptr_t));
        switch (static_cast<code>(opcode)) {
          case code::start_manager:
            mpx_->do_start(as_mgr(ptr));
            break;
          case code::run_action:
            add_action(ptr);
            break;
          case code::shutdown:
            CAF_ASSERT(ptr == 0);
            mpx_->do_shutdown();
            break;
          default:
            CAF_LOG_ERROR("opcode not recognized: " << CAF_ARG(opcode));
            break;
        }
      }
    } else if (num_bytes == 0) {
      CAF_LOG_DEBUG("pipe closed, assume shutdown");
      owner_->deregister();
      return;
    } else if (net::last_socket_error_is_temporary()) {
      return;
    } else {
      CAF_LOG_ERROR("pollset updater failed to read from the pipe");
      owner_->deregister();
      return;
    }
  }
}

void pollset_updater::handle_write_event() {
  owner_->deregister_writing();
}

void pollset_updater::abort(const error&) {
  // nop
}

} // namespace caf::detail
