// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/pollset_updater.hpp"

#include <cstring>

#include "caf/action.hpp"
#include "caf/actor_system.hpp"
#include "caf/logger.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/sec.hpp"
#include "caf/span.hpp"
#include "caf/variant.hpp"

namespace caf::net {

pollset_updater::pollset_updater(pipe_socket read_handle, multiplexer* parent)
  : super(read_handle, parent) {
  mask_ = operation::read;
}

pollset_updater::~pollset_updater() {
  // nop
}

error pollset_updater::init(const settings&) {
  CAF_LOG_TRACE("");
  return nonblocking(handle(), true);
}

namespace {

auto as_mgr(intptr_t ptr) {
  CAF_LOG_TRACE(CAF_ARG(ptr));
  return intrusive_ptr{reinterpret_cast<socket_manager*>(ptr), false};
};

void run_action(intptr_t ptr) {
  CAF_LOG_TRACE(CAF_ARG(ptr));
  auto f = action{intrusive_ptr{reinterpret_cast<action::impl*>(ptr), false}};
  f.run();
};

} // namespace

bool pollset_updater::handle_read_event() {
  CAF_LOG_TRACE("");
  for (;;) {
    CAF_ASSERT((buf_.size() - buf_size_) > 0);
    auto num_bytes = read(handle(), make_span(buf_.data() + buf_size_,
                                              buf_.size() - buf_size_));
    if (num_bytes > 0) {
      buf_size_ += static_cast<size_t>(num_bytes);
      if (buf_.size() == buf_size_) {
        buf_size_ = 0;
        auto opcode = static_cast<uint8_t>(buf_[0]);
        intptr_t ptr;
        memcpy(&ptr, buf_.data() + 1, sizeof(intptr_t));
        switch (static_cast<code>(opcode)) {
          case code::register_reading:
            parent_->register_reading(as_mgr(ptr));
            break;
          case code::register_writing:
            parent_->register_writing(as_mgr(ptr));
            break;
          case code::init_manager:
            parent_->init(as_mgr(ptr));
            break;
          case code::discard_manager:
            parent_->discard(as_mgr(ptr));
            break;
          case code::shutdown_reading:
            parent_->shutdown_reading(as_mgr(ptr));
            break;
          case code::shutdown_writing:
            parent_->shutdown_writing(as_mgr(ptr));
            break;
          case code::run_action:
            run_action(ptr);
            break;
          case code::shutdown:
            CAF_ASSERT(ptr == 0);
            parent_->shutdown();
            break;
          default:
            CAF_LOG_ERROR("opcode not recognized: " << CAF_ARG(opcode));
            break;
        }
      }
    } else if (num_bytes == 0) {
      CAF_LOG_DEBUG("pipe closed, assume shutdown");
      return false;
    } else {
      return last_socket_error_is_temporary();
    }
  }
}

bool pollset_updater::handle_write_event() {
  return false;
}

void pollset_updater::handle_error(sec) {
  // nop
}

void pollset_updater::continue_reading() {
  register_reading();
}

} // namespace caf::net
