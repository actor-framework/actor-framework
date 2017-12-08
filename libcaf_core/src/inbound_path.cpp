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

inbound_path::stats_t::stats_t() {
  // we initially assume one item = 1us
  // TODO: put constant in some header
  measurement x{50, std::chrono::microseconds(50)};
  measurements.resize(stats_sampling_size, x);
}

auto inbound_path::stats_t::calculate(std::chrono::nanoseconds c,
                                      std::chrono::nanoseconds d)
  -> calculation_result {
  // Max throughput = C * (N / t), where C = cycle length, N = measured items,
  // and t = measured time. Desired batch size is the same formula with D
  // instead of C.
  long total_ns = 0;
  long total_items = 0;
  for (auto& x : measurements) {
    total_ns += static_cast<long>(x.calculation_time.count());
    total_items += x.batch_size;
  }
  // Instead of C * (N / t) we calculate N * (C / t) to avoid imprecisions due
  // to double rounding for very small numbers.
  auto ct = static_cast<double>(c.count()) / total_ns;
  auto dt = static_cast<double>(d.count()) / total_ns;
  return {std::max(static_cast<long>(total_items * ct), 1l),
          std::max(static_cast<long>(total_items * dt), 1l)};
}

void inbound_path::stats_t::store(measurement x) {
  measurements[ring_iter] = x;
  ring_iter = (ring_iter + 1) % stats_sampling_size;
}

inbound_path::inbound_path(stream_manager_ptr mgr_ptr, stream_slots id,
                           strong_actor_ptr ptr)
    : mgr(std::move(mgr_ptr)),
      hdl(std::move(ptr)),
      slots(id),
      assigned_credit(0),
      prio(stream_priority::normal),
      last_acked_batch_id(0),
      last_batch_id(0),
      redeployable(false) {
  mgr->register_input_path(this);
}

inbound_path::~inbound_path() {
  mgr->deregister_input_path(this);
}

void inbound_path::handle(downstream_msg::batch& x) {
  assigned_credit -= x.xs_size;
  last_batch_id = x.id;
  // TODO: add time-measurement functions to local actor and store taken
  //       measurement in stats
  mgr->handle(this, x);
  mgr->push();
}

void inbound_path::emit_ack_open(local_actor* self, actor_addr rebind_from,
                                 bool is_redeployable) {
  assigned_credit = 50; // TODO: put constant in some header
  redeployable = is_redeployable;
  int32_t desired_batch_size = 50; // TODO: put constant in some header
  unsafe_send_as(self, hdl,
                 make<upstream_msg::ack_open>(
                   slots.invert(), self->address(), std::move(rebind_from),
                   self->ctrl(), static_cast<int32_t>(assigned_credit),
                   desired_batch_size, is_redeployable));
}

void inbound_path::emit_ack_batch(local_actor* self, long queued_items,
                                  std::chrono::nanoseconds cycle,
                                  std::chrono::nanoseconds complexity){
  auto x = stats.calculate(cycle, complexity);
  auto credit = std::max(x.max_throughput - (assigned_credit - queued_items),
                         0l);
  auto batch_size = static_cast<int32_t>(x.items_per_batch);
  if (credit != 0)
    assigned_credit += credit;
  unsafe_send_as(self, hdl,
                 make<upstream_msg::ack_batch>(slots.invert(), self->address(),
                                               static_cast<int32_t>(credit),
                                               batch_size, last_batch_id));
}

void inbound_path::emit_regular_shutdown(local_actor* self) {
  unsafe_send_as(self, hdl, make<upstream_msg::drop>(slots, self->address()));
}

void inbound_path::emit_regular_shutdown(local_actor* self, error reason) {
  unsafe_send_as(self, hdl,
                 make<upstream_msg::forced_drop>(
                   slots.invert(), self->address(), std::move(reason)));
}

void inbound_path::emit_irregular_shutdown(local_actor* self,
                                           stream_slots slots,
                                           const strong_actor_ptr& hdl,
                                           error reason) {
  unsafe_send_as(self, hdl,
                 make<upstream_msg::forced_drop>(
                   slots.invert(), self->address(), std::move(reason)));
}

} // namespace caf
