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

#include "caf/net/pollset_updater.hpp"

#include <cstring>

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
  return nonblocking(handle(), true);
}

bool pollset_updater::handle_read_event() {
  for (;;) {
    auto num_bytes = read(handle(), make_span(buf_.data() + buf_size_,
                                              buf_.size() - buf_size_));
    if (num_bytes > 0) {
      buf_size_ += static_cast<size_t>(num_bytes);
      if (buf_.size() == buf_size_) {
        buf_size_ = 0;
        auto opcode = static_cast<uint8_t>(buf_[0]);
        intptr_t value;
        memcpy(&value, buf_.data() + 1, sizeof(intptr_t));
        socket_manager_ptr mgr{reinterpret_cast<socket_manager*>(value), false};
        switch (opcode) {
          case 0:
            parent_->register_reading(mgr);
            break;
          case 1:
            parent_->register_writing(mgr);
            break;
          case 4:
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

} // namespace caf::net
