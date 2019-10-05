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

#include "caf/net/multiplexer.hpp"
#include "caf/sec.hpp"
#include "caf/span.hpp"
#include "caf/variant.hpp"

namespace caf {
namespace net {

pollset_updater::pollset_updater(pipe_socket read_handle,
                                 multiplexer_ptr parent)
  : super(read_handle, std::move(parent)), buf_size_(0) {
  mask_ = operation::read;
  nonblocking(read_handle, true);
}

pollset_updater::~pollset_updater() {
  // nop
}

bool pollset_updater::handle_read_event() {
  for (;;) {
    auto res = read(handle(), make_span(buf_.data() + buf_size_,
                                        buf_.size() - buf_size_));
    if (auto num_bytes = get_if<size_t>(&res)) {
      buf_size_ += *num_bytes;
      if (buf_.size() == buf_size_) {
        buf_size_ = 0;
        auto value = *reinterpret_cast<intptr_t*>(buf_.data());
        socket_manager_ptr mgr{reinterpret_cast<socket_manager*>(value), false};
        if (auto ptr = parent_.lock())
          ptr->register_writing(mgr);
      }
    } else {
      return get<sec>(res) == sec::unavailable_or_would_block;
    }
  }
}

bool pollset_updater::handle_write_event() {
  return false;
}

void pollset_updater::handle_error(sec) {
  // nop
}

} // namespace net
} // namespace caf
