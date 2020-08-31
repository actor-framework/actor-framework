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
#include "caf/detail/meta_object.hpp"
#include "caf/logger.hpp"
#include "caf/no_stages.hpp"
#include "caf/scheduled_actor.hpp"
#include "caf/send.hpp"
#include "caf/settings.hpp"

namespace {

template <class T>
void set_controller(std::unique_ptr<caf::credit_controller>& ptr,
                    caf::local_actor* self) {
  ptr = std::make_unique<T>(self);
}

} // namespace

namespace caf {

// -- constructors, destructors, and assignment operators ----------------------

inbound_path::inbound_path(stream_manager* ptr, type_id_t in_type) : mgr(ptr) {
  // Note: we can't include stream_manager.hpp in the header of inbound_path,
  // because that would cause a circular reference. Hence, we also can't use an
  // intrusive_ptr as member and instead call `ref/deref` manually.
  CAF_ASSERT(mgr != nullptr);
  mgr->ref();
  auto self = mgr->self();
  auto [processed_elements, input_buffer_size]
    = self->inbound_stream_metrics(in_type);
  metrics = metrics_t{processed_elements, input_buffer_size};
  mgr->register_input_path(this);
  CAF_STREAM_LOG_DEBUG(self->name()
                       << "opens input stream with element type"
                       << detail::global_meta_object(in_type)->type_name);
  last_ack_time = self->now();
}

inbound_path::~inbound_path() {
  mgr->deregister_input_path(this);
  mgr->deref();
}

void inbound_path::init(strong_actor_ptr source_hdl, stream_slots id) {
  hdl = std::move(source_hdl);
  slots = id;
}

// -- properties ---------------------------------------------------------------

bool inbound_path::up_to_date() const noexcept {
  return last_acked_batch_id == last_batch_id;
}

scheduled_actor* inbound_path::self() const noexcept {
  return mgr->self();
}

int32_t inbound_path::available_credit() const noexcept {
  // The max_credit may decrease as a result of re-calibration, in which case
  // the source can have more than the maximum amount for a brief period.
  return std::max(max_credit - assigned_credit, int32_t{0});
}

const settings& inbound_path::config() const noexcept {
  return content(mgr->self()->config());
}

// -- callbacks ----------------------------------------------------------------

void inbound_path::handle(downstream_msg::batch& batch) {
  CAF_LOG_TRACE(CAF_ARG(slots) << CAF_ARG(batch));
  // Handle batch.
  auto batch_size = batch.xs_size;
  last_batch_id = batch.id;
  CAF_STREAM_LOG_DEBUG(self()->name() << "handles batch of size" << batch_size
                                      << "on slot" << slots.receiver << "with"
                                      << assigned_credit << "assigned credit");
  assigned_credit -= batch_size;
  CAF_ASSERT(assigned_credit >= 0);
  controller_->before_processing(batch);
  mgr->handle(this, batch);
  // Update settings as necessary.
  if (--calibration_countdown == 0) {
    auto [cmax, bsize, countdown] = controller_->calibrate();
    max_credit = cmax;
    desired_batch_size = bsize;
    calibration_countdown = countdown;
  }
  // Send ACK whenever we can assign credit for another batch to the source.
  if (auto available = available_credit(); available >= desired_batch_size)
    if (auto acquired = mgr->acquire_credit(this, available); acquired > 0)
      emit_ack_batch(self(), acquired);
  // FIXME: move this up to the actor
  mgr->push();
}

void inbound_path::tick(time_point now, duration_type max_batch_delay) {
  if (now >= last_ack_time + (2 * max_batch_delay)) {
    int32_t new_credit = 0;
    if (auto available = available_credit(); available > 0)
      new_credit = mgr->acquire_credit(this, available);
    emit_ack_batch(self(), new_credit);
  }
}

void inbound_path::handle(downstream_msg::close& x) {
  mgr->handle(this, x);
}

void inbound_path::handle(downstream_msg::forced_close& x) {
  mgr->handle(this, x);
}

// -- messaging ----------------------------------------------------------------

void inbound_path::emit_ack_open(local_actor* self, actor_addr rebind_from) {
  CAF_LOG_TRACE(CAF_ARG(slots) << CAF_ARG(rebind_from));
  // Update state.
  auto [cmax, bsize, countdown] = controller_->init();
  max_credit = cmax;
  desired_batch_size = bsize;
  calibration_countdown = countdown;
  if (auto available = available_credit(); available > 0)
    if (auto acquired = mgr->acquire_credit(this, available); acquired > 0)
      assigned_credit += acquired;
  // Make sure we receive errors from this point on.
  stream_aborter::add(hdl, self->address(), slots.receiver,
                      stream_aborter::source_aborter);
  // Send message.
  unsafe_send_as(self, hdl,
                 make<upstream_msg::ack_open>(
                   slots.invert(), self->address(), std::move(rebind_from),
                   self->ctrl(), assigned_credit, desired_batch_size));
  last_ack_time = self->now();
}

void inbound_path::emit_ack_batch(local_actor* self, int32_t new_credit) {
  CAF_LOG_TRACE(CAF_ARG(new_credit));
  CAF_ASSERT(desired_batch_size > 0);
  if (last_acked_batch_id == last_batch_id && new_credit == 0)
    return;
  unsafe_send_as(self, hdl,
                 make<upstream_msg::ack_batch>(slots.invert(), self->address(),
                                               new_credit, desired_batch_size,
                                               last_batch_id));
  last_acked_batch_id = last_batch_id;
  assigned_credit += new_credit;
  last_ack_time = self->now();
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

} // namespace caf
