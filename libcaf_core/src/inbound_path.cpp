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
#include "caf/scheduled_actor.hpp"

namespace caf {

inbound_path::stats_t::stats_t() : ring_iter(0) {
  measurement x{0, timespan{0}};
  measurements.resize(stats_sampling_size, x);
}

auto inbound_path::stats_t::calculate(timespan c, timespan d)
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
  if (total_ns == 0)
    return {1, 1};
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
      last_batch_id(0) {
  mgr->register_input_path(this);
}

inbound_path::~inbound_path() {
  mgr->deregister_input_path(this);
}

void inbound_path::handle(downstream_msg::batch& x) {
  CAF_LOG_TRACE(CAF_ARG(slots) << CAF_ARG(x));
  auto batch_size = x.xs_size;
  assigned_credit -= batch_size;
  last_batch_id = x.id;
  auto& clock = mgr->self()->clock();
  auto t0 = clock.now();
  mgr->handle(this, x);
  auto t1 = clock.now();
  auto dt = clock.difference(atom("batch"), batch_size, t0, t1);
  stats.store({batch_size, dt});
  mgr->push();
}

void inbound_path::emit_ack_open(local_actor* self, actor_addr rebind_from) {
  CAF_LOG_TRACE(CAF_ARG(slots) << CAF_ARG(rebind_from));
  assigned_credit = initial_credit;
  int32_t desired_batch_size = initial_credit;
  unsafe_send_as(self, hdl,
                 make<upstream_msg::ack_open>(
                   slots.invert(), self->address(), std::move(rebind_from),
                   self->ctrl(), static_cast<int32_t>(assigned_credit),
                   desired_batch_size));
}

void inbound_path::emit_ack_batch(local_actor* self, long queued_items,
                                  timespan cycle, timespan complexity) {
  CAF_LOG_TRACE(CAF_ARG(slots) << CAF_ARG(queued_items) << CAF_ARG(cycle)
                << CAF_ARG(complexity));
  if (up_to_date())
    return;
  auto x = stats.calculate(cycle, complexity);
  // Hand out enough credit to fill our queue for 2 cycles.
  auto credit = std::max((x.max_throughput * 2)
                         - (assigned_credit - queued_items),
                         0l);
  auto batch_size = static_cast<int32_t>(x.items_per_batch);
  if (credit != 0)
    assigned_credit += credit;
  CAF_LOG_DEBUG(CAF_ARG(credit) << CAF_ARG(batch_size));
  unsafe_send_as(self, hdl,
                 make<upstream_msg::ack_batch>(slots.invert(),
                                               self->address(),
                                               static_cast<int32_t>(credit),
                                               batch_size, last_batch_id));
  last_acked_batch_id = last_batch_id;
}

bool inbound_path::up_to_date() {
  return last_acked_batch_id == last_batch_id;
}

void inbound_path::emit_regular_shutdown(local_actor* self) {
  CAF_LOG_TRACE(CAF_ARG(slots));
  unsafe_send_as(self, hdl, make<upstream_msg::drop>(slots, self->address()));
}

void inbound_path::emit_irregular_shutdown(local_actor* self, error reason) {
  CAF_LOG_TRACE(CAF_ARG(slots) << CAF_ARG(reason));
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
