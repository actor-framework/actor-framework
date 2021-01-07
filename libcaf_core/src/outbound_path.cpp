// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/outbound_path.hpp"

#include "caf/local_actor.hpp"
#include "caf/logger.hpp"
#include "caf/no_stages.hpp"
#include "caf/send.hpp"

namespace caf {

namespace {

// TODO: consider making this parameter configurable
constexpr int32_t max_batch_size = 128 * 1024;

} // namespace

outbound_path::outbound_path(stream_slot sender_slot,
                             strong_actor_ptr receiver_hdl)
  : slots(sender_slot, invalid_stream_slot),
    hdl(std::move(receiver_hdl)),
    next_batch_id(1),
    open_credit(0),
    desired_batch_size(50),
    next_ack_id(1),
    closing(false) {
  // nop
}

outbound_path::~outbound_path() {
  // nop
}

void outbound_path::emit_batch(local_actor* self, int32_t xs_size, message xs) {
  CAF_LOG_TRACE(CAF_ARG(slots) << CAF_ARG(xs_size) << CAF_ARG(xs));
  CAF_ASSERT(xs_size > 0);
  CAF_ASSERT(xs_size <= std::numeric_limits<int32_t>::max());
  CAF_ASSERT(open_credit >= xs_size);
  open_credit -= xs_size;
  CAF_ASSERT(open_credit >= 0);
  auto bid = next_batch_id++;
  downstream_msg::batch batch{static_cast<int32_t>(xs_size), std::move(xs),
                              bid};
  unsafe_send_as(self, hdl,
                 downstream_msg{slots, self->address(), std::move(batch)});
}

void outbound_path::emit_regular_shutdown(local_actor* self) {
  CAF_LOG_TRACE(CAF_ARG(slots));
  unsafe_send_as(self, hdl,
                 make<downstream_msg::close>(slots, self->address()));
}

void outbound_path::emit_irregular_shutdown(local_actor* self, error reason) {
  CAF_LOG_TRACE(CAF_ARG(slots) << CAF_ARG(reason));
  /// Note that we always send abort messages anonymous. They can get send
  /// after `self` already terminated and we must not form strong references
  /// after that point. Since downstream messages contain the sender address
  /// anyway, we only omit redundant information.
  anon_send(actor_cast<actor>(hdl),
            make<downstream_msg::forced_close>(slots, self->address(),
                                               std::move(reason)));
}

void outbound_path::emit_irregular_shutdown(local_actor* self,
                                            stream_slots slots,
                                            const strong_actor_ptr& hdl,
                                            error reason) {
  CAF_LOG_TRACE(CAF_ARG(slots) << CAF_ARG(hdl) << CAF_ARG(reason));
  /// Note that we always send abort messages anonymous. See reasoning in first
  /// function overload.
  anon_send(actor_cast<actor>(hdl),
            make<downstream_msg::forced_close>(slots, self->address(),
                                               std::move(reason)));
}

void outbound_path::set_desired_batch_size(int32_t value) noexcept {
  if (value == desired_batch_size)
    return;
  desired_batch_size = value < 0 || value > max_batch_size ? max_batch_size
                                                           : value;
}

} // namespace caf
