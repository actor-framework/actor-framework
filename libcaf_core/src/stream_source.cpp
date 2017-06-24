/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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
#include "caf/downstream_policy.hpp"

namespace caf {

stream_source::stream_source(downstream_policy* out_ptr) : out_ptr_(out_ptr) {
  // nop
}

stream_source::~stream_source() {
  // nop
}

bool stream_source::done() const {
  // we are done streaming if no downstream paths remain
  return out_ptr_->closed();
}

error stream_source::downstream_ack(strong_actor_ptr& hdl, int64_t,
                                    long demand) {
  CAF_LOG_TRACE(CAF_ARG(hdl) << CAF_ARG(demand));
  auto path = out_ptr_->find(hdl);
  if (path) {
    path->open_credit += demand;
    if (!at_end()) {
      // produce new elements
      auto capacity = out().credit();
      // TODO: fix issue where a source does not emit the last pieces if
      //       min_batch_size cannot be reached
      while (capacity > out().min_batch_size()) {
        auto current_size = buf_size();
        auto size_hint = std::min(capacity, out().max_batch_size());
        if (size_hint > current_size)
          generate(static_cast<size_t>(size_hint - current_size));
        auto err = push();
        if (err)
          return err;
        // Check if `push` did actually use credit, otherwise break loop
        auto new_capacity = out().credit();
        if (new_capacity != capacity)
          capacity = new_capacity;
        else
          break;
      }
      return none;
    }
    // transmit cached elements before closing paths
    if (buf_size() > 0)
      return push();
    if (out_ptr_->remove_path(hdl))
      return none;
  }
  return sec::invalid_downstream;
}

void stream_source::abort(strong_actor_ptr& cause, const error& reason) {
  out_ptr_->abort(cause, reason);
}

void stream_source::generate() {
  CAF_LOG_TRACE("");
  if (!at_end()) {
    auto current_size = buf_size();
    auto size_hint = out().credit();
    if (current_size < size_hint)
      generate(static_cast<size_t>(size_hint - current_size));
  }
}

void stream_source::downstream_demand(downstream_path* path, long demand) {
  CAF_ASSERT(path != nullptr);
  CAF_LOG_TRACE(CAF_ARG(path) << CAF_ARG(demand));
  path->open_credit += demand;
  if (!at_end()) {
    // produce new elements
    auto capacity = out().credit();
    // TODO: fix issue where a source does not emit the last pieces if
    //       min_batch_size cannot be reached
    while (capacity > out().min_batch_size()) {
      auto current_size = buf_size();
      auto size_hint = std::min(capacity, out().max_batch_size());
      if (size_hint > current_size)
        generate(static_cast<size_t>(size_hint - current_size));
      push();
      // Check if `push` did actually use credit, otherwise break loop
      auto new_capacity = out().credit();
      if (new_capacity != capacity)
        capacity = new_capacity;
      else
        break;
    }
    return;
  }
  // transmit cached elements before closing paths
  if (buf_size() > 0)
    push();
  auto hdl = path->hdl;
  out_ptr_->remove_path(hdl);
}

} // namespace caf
