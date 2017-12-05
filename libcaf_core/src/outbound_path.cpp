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

#include "caf/outbound_path.hpp"

#include "caf/send.hpp"
#include "caf/logger.hpp"
#include "caf/no_stages.hpp"
#include "caf/local_actor.hpp"

namespace caf {

outbound_path::outbound_path(stream_slots id, strong_actor_ptr ptr)
    : slots(id),
      hdl(std::move(ptr)),
      next_batch_id(0),
      open_credit(0),
      desired_batch_size(0),
      redeployable(false),
      next_ack_id(0) {
  // nop
}

outbound_path::~outbound_path() {
  // nop
}

void outbound_path::emit_batch(local_actor* self, long xs_size, message xs) {
  CAF_ASSERT(open_credit >= xs_size);
  open_credit -= xs_size;
  auto bid = next_batch_id++;
  downstream_msg::batch batch{static_cast<int32_t>(xs_size), std::move(xs),
                              bid};
  if (redeployable)
    unacknowledged_batches.emplace_back(bid, batch);
  unsafe_send_as(self, hdl,
                 downstream_msg{slots, self->address(), std::move(batch)});
}

void outbound_path::emit_regular_shutdown(local_actor* self) {
  unsafe_send_as(self, hdl,
                 make<downstream_msg::close>(slots, self->address()));
}

void outbound_path::emit_irregular_shutdown(local_actor* self, error reason) {
  unsafe_send_as(self, hdl,
                 make<downstream_msg::forced_close>(slots, self->address(),
                                                    std::move(reason)));
}

void outbound_path::emit_irregular_shutdown(local_actor* self,
                                            stream_slots slots,
                                            const strong_actor_ptr& hdl,
                                            error reason) {
  unsafe_send_as(self, hdl,
                 make<downstream_msg::forced_close>(slots, self->address(),
                                                    std::move(reason)));
}

} // namespace caf
