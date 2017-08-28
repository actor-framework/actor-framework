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

#include "caf/inbound_path.hpp"

#include "caf/send.hpp"
#include "caf/logger.hpp"
#include "caf/no_stages.hpp"
#include "caf/local_actor.hpp"

namespace caf {

inbound_path::inbound_path(local_actor* selfptr, const stream_id& id,
                           strong_actor_ptr ptr)
    : self(selfptr),
      sid(std::move(id)),
      hdl(std::move(ptr)),
      prio(stream_priority::normal),
      last_acked_batch_id(0),
      last_batch_id(0),
      assigned_credit(0),
      redeployable(false) {
  // nop
}

inbound_path::~inbound_path() {
  if (hdl) {
    if (shutdown_reason == none)
      unsafe_send_as(self, hdl, make<stream_msg::drop>(sid, self->address()));
    else
      unsafe_send_as(self, hdl,
                     make<stream_msg::forced_drop>(sid, self->address(),
                                                   shutdown_reason));
  }
}

void inbound_path::handle_batch(long batch_size, int64_t batch_id) {
  assigned_credit -= batch_size;
  last_batch_id = batch_id;
}

void inbound_path::emit_ack_open(actor_addr rebind_from,
                                 long initial_demand, bool is_redeployable) {
  CAF_LOG_TRACE(CAF_ARG(rebind_from) << CAF_ARG(initial_demand)
                << CAF_ARG(is_redeployable));
  assigned_credit = initial_demand;
  redeployable = is_redeployable;
  unsafe_send_as(self, hdl,
                 make<stream_msg::ack_open>(
                   sid, self->address(), std::move(rebind_from), self->ctrl(),
                   static_cast<int32_t>(initial_demand), is_redeployable));
}

void inbound_path::emit_ack_batch(long new_demand) {
  CAF_LOG_TRACE(CAF_ARG(new_demand));
  last_acked_batch_id = last_batch_id;
  assigned_credit += new_demand;
  unsafe_send_as(self, hdl,
                 make<stream_msg::ack_batch>(sid, self->address(),
                                             static_cast<int32_t>(new_demand),
                                             last_batch_id));
}

void inbound_path::emit_irregular_shutdown(local_actor* self,
                                           const stream_id& sid,
                                           const strong_actor_ptr& hdl,
                                           error reason) {
  CAF_LOG_TRACE(CAF_ARG(sid) << CAF_ARG(hdl) << CAF_ARG(reason));
  unsafe_send_as(self, hdl, make<stream_msg::forced_drop>(sid, self->address(),
                                                          std::move(reason)));
}

} // namespace caf
