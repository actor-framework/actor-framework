/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
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

inbound_path::inbound_path(stream_manager_ptr mgr_ptr, stream_slots id,
                           strong_actor_ptr ptr)
    : mgr(std::move(mgr_ptr)),
      slots(id),
      hdl(std::move(ptr)),
      prio(stream_priority::normal),
      last_acked_batch_id(0),
      last_batch_id(0),
      assigned_credit(0),
      desired_batch_size(50), // TODO: at least put default in some header
      redeployable(false) {
  mgr->register_input_path(this);
}

inbound_path::~inbound_path() {
  mgr->deregister_input_path(this);
}

void inbound_path::handle_batch(long batch_size, int64_t batch_id) {
  assigned_credit -= batch_size;
  last_batch_id = batch_id;
}

void inbound_path::emit_ack_open(local_actor* self, actor_addr rebind_from,
                                 long initial_demand, bool is_redeployable) {
  assigned_credit = initial_demand;
  redeployable = is_redeployable;
  auto batch_size = static_cast<int32_t>(desired_batch_size);
  unsafe_send_as(self, hdl,
                 make<upstream_msg::ack_open>(
                   slots, self->address(), std::move(rebind_from), self->ctrl(),
                   static_cast<int32_t>(initial_demand), batch_size,
                   is_redeployable));
}

void inbound_path::emit_ack_batch(local_actor* self, long new_demand) {
  last_acked_batch_id = last_batch_id;
  assigned_credit += new_demand;
  auto batch_size = static_cast<int32_t>(desired_batch_size);
  unsafe_send_as(self, hdl,
                 make<upstream_msg::ack_batch>(slots, self->address(),
                                               static_cast<int32_t>(new_demand),
                                               batch_size, last_batch_id));
}

void inbound_path::emit_regular_shutdown(local_actor* self) {
  unsafe_send_as(self, hdl, make<upstream_msg::drop>(slots, self->address()));
}

void inbound_path::emit_regular_shutdown(local_actor* self, error reason) {
  unsafe_send_as(
    self, hdl,
    make<upstream_msg::forced_drop>(slots, self->address(), std::move(reason)));
}

void inbound_path::emit_irregular_shutdown(local_actor* self,
                                           stream_slots slots,
                                           const strong_actor_ptr& hdl,
                                           error reason) {
  unsafe_send_as(
    self, hdl,
    make<upstream_msg::forced_drop>(slots, self->address(), std::move(reason)));
}

} // namespace caf
