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

#include "caf/stream_stage.hpp"

#include "caf/sec.hpp"
#include "caf/logger.hpp"
#include "caf/downstream_path.hpp"
#include "caf/abstract_upstream.hpp"
#include "caf/downstream_policy.hpp"
#include "caf/abstract_downstream.hpp"

namespace caf {

stream_stage::stream_stage(abstract_upstream* in_ptr,
                           abstract_downstream* out_ptr)
    : in_ptr_(in_ptr),
      out_ptr_(out_ptr) {
  // nop
}

bool stream_stage::done() const {
  return in_ptr_->closed() && out_ptr_->closed();
}

error stream_stage::upstream_batch(strong_actor_ptr& hdl, size_t xs_size,
                     message& xs) {
  auto err = in_ptr_->pull(hdl, xs_size);
  if (!err) {
    process_batch(xs);
    strong_actor_ptr dummy;
    trigger_send(dummy);
  }
  return err;
}

error stream_stage::downstream_demand(strong_actor_ptr& hdl, size_t value) {
  auto path = out_ptr_->find(hdl);
  if (path) {
    path->open_credit += value;
    if(out_ptr_->buf_size() > 0) {
      return trigger_send(hdl);
    } if (in_ptr_->closed()) {
      if (!out_ptr_->remove_path(hdl)) {
        return sec::invalid_downstream;
      }
    }
    return none;
  }
  return sec::invalid_downstream;
}

void stream_stage::abort(strong_actor_ptr& cause, const error& reason) {
  in_ptr_->abort(cause, reason);
  out_ptr_->abort(cause, reason);
}

void stream_stage::last_upstream_closed() {
  if (out().buf_size() == 0)
    out().close();
}

} // namespace caf
