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

#include "caf/actor_system_config.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/complexity_based_credit_controller.hpp"
#include "caf/detail/size_based_credit_controller.hpp"
#include "caf/detail/test_credit_controller.hpp"
#include "caf/logger.hpp"
#include "caf/no_stages.hpp"
#include "caf/scheduled_actor.hpp"
#include "caf/send.hpp"
#include "caf/settings.hpp"

namespace caf {

namespace {

constexpr bool force_ack = true;

void emit_ack_batch(inbound_path& path, credit_controller::assignment x,
                    bool force_ack_msg = false) {
  CAF_ASSERT(x.batch_size > 0);
  auto& out = path.mgr->out();
  path.desired_batch_size = x.batch_size;
  int32_t new_credit = 0;
  auto used = static_cast<int32_t>(out.buffered()) + path.assigned_credit;
  auto guard = detail::make_scope_guard([&] {
    if (!force_ack_msg || path.up_to_date())
      return;
    unsafe_send_as(path.self(), path.hdl,
                   make<upstream_msg::ack_batch>(
                     path.slots.invert(), path.self()->address(), 0,
                     x.batch_size, path.last_batch_id, x.credit));
    path.last_acked_batch_id = path.last_batch_id;
  });
  if (x.credit <= used)
    return;
  new_credit = path.mgr->acquire_credit(&path, x.credit - used);
  if (new_credit < 1)
    return;
  guard.disable();
  unsafe_send_as(path.self(), path.hdl,
                 make<upstream_msg::ack_batch>(
                   path.slots.invert(), path.self()->address(), new_credit,
                   x.batch_size, path.last_batch_id, x.credit));
  path.last_acked_batch_id = path.last_batch_id;
  path.assigned_credit += new_credit;
}

} // namespace

inbound_path::inbound_path(stream_manager_ptr mgr_ptr, stream_slots id,
                           strong_actor_ptr ptr, rtti_pair in_type)
  : mgr(std::move(mgr_ptr)), hdl(std::move(ptr)), slots(id) {
  CAF_IGNORE_UNUSED(in_type);
  mgr->register_input_path(this);
  CAF_STREAM_LOG_DEBUG(mgr->self()->name()
                       << "opens input stream with element type"
                       << mgr->self()->system().types().portable_name(in_type)
                       << "at slot" << id.receiver << "from" << hdl);
  if (auto str = get_if<std::string>(&self()->system().config(),
                                     "stream.credit-policy")) {
    if (*str == "testing")
      controller_.reset(new detail::test_credit_controller(self()));
    else if (*str == "size")
      controller_.reset(new detail::size_based_credit_controller(self()));
    else
      controller_.reset(new detail::complexity_based_credit_controller(self()));
  } else {
    controller_.reset(new detail::complexity_based_credit_controller(self()));
  }
}

inbound_path::~inbound_path() {
  mgr->deregister_input_path(this);
}

void inbound_path::handle(downstream_msg::batch& x) {
  CAF_LOG_TRACE(CAF_ARG(slots) << CAF_ARG(x));
  auto batch_size = x.xs_size;
  last_batch_id = x.id;
  CAF_STREAM_LOG_DEBUG(mgr->self()->name() << "handles batch of size"
                       << batch_size << "on slot" << slots.receiver << "with"
                       << assigned_credit << "assigned credit");
  if (assigned_credit <= batch_size) {
    assigned_credit = 0;
    // Do not log a message when "running out of credit" for the first batch
    // that can easily consume the initial credit in one shot.
    CAF_STREAM_LOG_DEBUG_IF(next_credit_decision.time_since_epoch().count() > 0,
                            mgr->self()->name()
                              << "ran out of credit at slot" << slots.receiver);
  } else {
    assigned_credit -= batch_size;
    CAF_ASSERT(assigned_credit >= 0);
  }
  auto threshold = controller_->threshold();
  if (threshold >= 0 && assigned_credit <= threshold)
    caf::emit_ack_batch(*this, controller_->compute_bridge());
  controller_->before_processing(x);
  mgr->handle(this, x);
  controller_->after_processing(x);
  mgr->push();
}

void inbound_path::emit_ack_open(local_actor* self, actor_addr rebind_from) {
  CAF_LOG_TRACE(CAF_ARG(slots) << CAF_ARG(rebind_from));
  // Update state.
  auto initial = controller_->compute_initial();
  assigned_credit = mgr->acquire_credit(this, initial.credit);
  CAF_ASSERT(assigned_credit >= 0);
  desired_batch_size = std::min(initial.batch_size, assigned_credit);
  // Make sure we receive errors from this point on.
  stream_aborter::add(hdl, self->address(), slots.receiver,
                      stream_aborter::source_aborter);
  // Send message.
  unsafe_send_as(self, hdl,
                 make<upstream_msg::ack_open>(
                   slots.invert(), self->address(), std::move(rebind_from),
                   self->ctrl(), assigned_credit, desired_batch_size));
  last_credit_decision = self->clock().now();
}

void inbound_path::emit_ack_batch(local_actor*, int32_t,
                                  actor_clock::time_point now, timespan cycle) {
  CAF_LOG_TRACE(CAF_ARG(slots) << CAF_ARG(cycle));
  last_credit_decision = now;
  next_credit_decision = now + cycle;
  auto max_capacity = static_cast<int32_t>(mgr->out().max_capacity());
  caf::emit_ack_batch(*this, controller_->compute(cycle, max_capacity),
                      force_ack);
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

scheduled_actor* inbound_path::self() {
  return mgr->self();
}

} // namespace caf
