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

inbound_path::stats_t::stats_t() : num_elements(0), processing_time(0) {
  // nop
}

auto inbound_path::stats_t::calculate(timespan c, timespan d)
  -> calculation_result {
  // Max throughput = C * (N / t), where C = cycle length, N = measured items,
  // and t = measured time. Desired batch size is the same formula with D
  // instead of C.
  // We compute our values in 64-bit for more precision before truncating to a
  // 32-bit integer type at the end.
  int64_t total_ns = processing_time.count();
  if (total_ns == 0)
    return {1, 1};
  /// Helper for truncating a 64-bit integer to a 32-bit integer with a minimum
  /// value of 1.
  auto clamp = [](int64_t x) -> int32_t {
    static constexpr auto upper_bound = std::numeric_limits<int32_t>::max();
    if (x > upper_bound)
      return upper_bound;
    if (x <= 0)
      return 1;
    return static_cast<int32_t>(x);
  };
  // Instead of C * (N / t) we calculate (C * N) / t to avoid double conversion
  // and rounding errors.
  return {clamp((c.count() * num_elements) / total_ns),
          clamp((d.count() * num_elements) / total_ns)};
}

void inbound_path::stats_t::store(measurement x) {
  num_elements += x.batch_size;
  processing_time += x.calculation_time;
}

void inbound_path::stats_t::reset() {
  num_elements = 0;
  processing_time = timespan{0};
}

inbound_path::inbound_path(stream_manager_ptr mgr_ptr, stream_slots id,
                           strong_actor_ptr ptr, rtti_pair in_type)
    : mgr(std::move(mgr_ptr)),
      hdl(std::move(ptr)),
      slots(id),
      desired_batch_size(initial_credit),
      assigned_credit(0),
      prio(stream_priority::normal),
      last_acked_batch_id(0),
      last_batch_id(0) {
  CAF_IGNORE_UNUSED(in_type);
  mgr->register_input_path(this);
  CAF_STREAM_LOG_DEBUG(mgr->self()->name()
                       << "opens input stream with element type"
                       << *mgr->self()->system().types().portable_name(in_type)
                       << "at slot" << id.receiver << "from" << hdl);
}

inbound_path::~inbound_path() {
  mgr->deregister_input_path(this);
}

void inbound_path::handle(downstream_msg::batch& x) {
  CAF_LOG_TRACE(CAF_ARG(slots) << CAF_ARG(x));
  auto& clk = clock();
  auto batch_size = x.xs_size;
  last_batch_id = x.id;
  auto t0 = clk.now();
  CAF_STREAM_LOG_DEBUG(mgr->self()->name() << "handles batch of size"
                       << batch_size << "on slot" << slots.receiver << "with"
                       << assigned_credit << "assigned credit");
  if (assigned_credit <= batch_size) {
    assigned_credit = 0;
    // Do not log a message when "running out of credit" for the first batch
    // that can easily consume the initial credit in one shot.
    CAF_STREAM_LOG_DEBUG_IF(next_credit_decision.time_since_epoch().count() > 0,
                            mgr->self()->name() << "ran out of credit at slot"
                            << slots.receiver << "with approx."
                            << (next_credit_decision - t0)
                            << "until next cycle");
  } else {
    assigned_credit -= batch_size;
    CAF_ASSERT(assigned_credit >= 0);
  }
  mgr->handle(this, x);
  auto t1 = clk.now();
  auto dt = clk.difference(atom("batch"), batch_size, t0, t1);
  stats.store({batch_size, dt});
  mgr->push();
}

void inbound_path::emit_ack_open(local_actor* self, actor_addr rebind_from) {
  CAF_LOG_TRACE(CAF_ARG(slots) << CAF_ARG(rebind_from));
  // Update state.
  assigned_credit = mgr->acquire_credit(this, initial_credit);
  CAF_ASSERT(assigned_credit >= 0);
  // Make sure we receive errors from this point on.
  stream_aborter::add(hdl, self->address(), slots.receiver,
                      stream_aborter::source_aborter);
  // Send message.
  unsafe_send_as(self, hdl,
                 make<upstream_msg::ack_open>(
                   slots.invert(), self->address(), std::move(rebind_from),
                   self->ctrl(), assigned_credit, desired_batch_size));
  last_credit_decision = clock().now();
}

void inbound_path::emit_ack_batch(local_actor* self, int32_t queued_items,
                                  int32_t max_downstream_capacity,
                                  actor_clock::time_point now, timespan cycle,
                                  timespan complexity) {
  CAF_LOG_TRACE(CAF_ARG(slots) << CAF_ARG(queued_items)
                << CAF_ARG(max_downstream_capacity) << CAF_ARG(cycle)
                << CAF_ARG(complexity));
  CAF_IGNORE_UNUSED(queued_items);
  // Update timestamps.
  last_credit_decision = now;
  next_credit_decision = now + cycle;
  // Hand out enough credit to fill our queue for 2 cycles but never exceed
  // the downstream capacity.
  auto x = stats.calculate(cycle, complexity);
  auto stats_guard = detail::make_scope_guard([&] { stats.reset(); });
  auto max_capacity = std::min(x.max_throughput * 2, max_downstream_capacity);
  CAF_ASSERT(max_capacity > 0);
  // Protect against overflow on `assigned_credit`.
  auto max_new_credit = std::numeric_limits<int32_t>::max() - assigned_credit;
  // Compute the amount of credit we grant in this round.
  auto credit = std::min(std::max(max_capacity - assigned_credit, 0),
                         max_new_credit);
  CAF_ASSERT(credit >= 0);
  // The manager can restrict or adjust the amount of credit.
  credit = std::min(mgr->acquire_credit(this, credit), max_new_credit);
  CAF_STREAM_LOG_DEBUG(mgr->self()->name() << "grants" << credit
                       << "new credit at slot" << slots.receiver
                       << "after receiving" << stats.num_elements
                       << "elements that took" << stats.processing_time
                       << CAF_ARG2("max_throughput", x.max_throughput)
                       << CAF_ARG(max_downstream_capacity)
                       << CAF_ARG(assigned_credit));
  if (credit == 0 && up_to_date())
    return;
  CAF_LOG_DEBUG(CAF_ARG(assigned_credit) << CAF_ARG(max_capacity)
                << CAF_ARG(queued_items) << CAF_ARG(credit)
                << CAF_ARG(desired_batch_size));
  assigned_credit += credit;
  CAF_ASSERT(assigned_credit >= 0);
  desired_batch_size = static_cast<int32_t>(x.items_per_batch);
  unsafe_send_as(self, hdl,
                 make<upstream_msg::ack_batch>(slots.invert(), self->address(),
                                               static_cast<int32_t>(credit),
                                               desired_batch_size,
                                               last_batch_id, max_capacity));
  last_acked_batch_id = last_batch_id;
}

bool inbound_path::up_to_date() {
  return last_acked_batch_id == last_batch_id;
}

void inbound_path::emit_regular_shutdown(local_actor* self) {
  CAF_LOG_TRACE(CAF_ARG(slots));
  unsafe_send_as(self, hdl,
                 make<upstream_msg::drop>(slots.invert(), self->address()));
}

void inbound_path::emit_irregular_shutdown(local_actor* self, error reason) {
  CAF_LOG_TRACE(CAF_ARG(slots) << CAF_ARG(reason));
  /// Note that we always send abort messages anonymous. They can get send
  /// after `self` already terminated and we must not form strong references
  /// after that point. Since upstream messages contain the sender address
  /// anyway, we only omit redundant information anyways.
  anon_send(actor_cast<actor>(hdl),
            make<upstream_msg::forced_drop>(slots.invert(), self->address(),
                                            std::move(reason)));
}

void inbound_path::emit_irregular_shutdown(local_actor* self,
                                           stream_slots slots,
                                           const strong_actor_ptr& hdl,
                                           error reason) {
  /// Note that we always send abort messages anonymous. See reasoning in first
  /// function overload.
  anon_send(actor_cast<actor>(hdl),
            make<upstream_msg::forced_drop>(slots.invert(), self->address(),
                                            std::move(reason)));
}

actor_clock& inbound_path::clock() {
  return mgr->self()->clock();
}

} // namespace caf
