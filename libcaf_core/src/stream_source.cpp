/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/stream_source.hpp"

#include "caf/sec.hpp"
#include "caf/error.hpp"
#include "caf/logger.hpp"
#include "caf/downstream_path.hpp"
#include "caf/abstract_downstream.hpp"

namespace caf {

stream_source::stream_source(abstract_downstream* out_ptr) : out_ptr_(out_ptr) {
  // nop
}

stream_source::~stream_source() {
  // nop
}

bool stream_source::done() const {
  // we are done streaming if no downstream paths remain
  return out_ptr_->closed();
}

error stream_source::downstream_demand(strong_actor_ptr& hdl, size_t value) {
  CAF_LOG_TRACE(CAF_ARG(hdl) << CAF_ARG(value));
  auto path = out_ptr_->find(hdl);
  if (path) {
    path->open_credit += value;
    if (!at_end()) {
      // produce new elements
      auto current_size = buf_size();
      auto size_hint = out_ptr_->policy().desired_buffer_size(*out_ptr_);
      if (current_size < size_hint)
        generate(size_hint - current_size);
      return trigger_send(hdl);
    } else if(buf_size() > 0) {
      // transmit cached elements before closing paths
      return trigger_send(hdl);
    } else if (out_ptr_->remove_path(hdl)) {
      return none;
    }
  }
  return sec::invalid_downstream;
}

void stream_source::abort(strong_actor_ptr& cause, const error& reason) {
  out_ptr_->abort(cause, reason);
}

} // namespace caf
