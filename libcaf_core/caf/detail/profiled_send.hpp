// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <vector>

#include "caf/actor_cast.hpp"
#include "caf/actor_clock.hpp"
#include "caf/actor_control_block.hpp"
#include "caf/actor_profiler.hpp"
#include "caf/fwd.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/message_id.hpp"
#include "caf/no_stages.hpp"

namespace caf::detail {

template <class Self, class SelfHandle, class Handle, class... Ts>
void profiled_send(Self* self, SelfHandle&& src, const Handle& dst,
                   message_id msg_id, std::vector<strong_actor_ptr> stages,
                   execution_unit* context, Ts&&... xs) {
  if (dst) {
    auto element = make_mailbox_element(std::forward<SelfHandle>(src), msg_id,
                                        std::move(stages),
                                        std::forward<Ts>(xs)...);
    CAF_BEFORE_SENDING(self, *element);
    dst->enqueue(std::move(element), context);
  } else {
    self->home_system().base_metrics().rejected_messages->inc();
  }
}

template <class Self, class SelfHandle, class Handle, class... Ts>
void profiled_send(Self* self, SelfHandle&& src, const Handle& dst,
                   actor_clock& clock, actor_clock::time_point timeout,
                   [[maybe_unused]] message_id msg_id, Ts&&... xs) {
  if (dst) {
    if constexpr (std::is_same<Handle, group>::value) {
      clock.schedule_message(timeout, dst, std::forward<SelfHandle>(src),
                             make_message(std::forward<Ts>(xs)...));
    } else {
      auto element = make_mailbox_element(std::forward<SelfHandle>(src), msg_id,
                                          no_stages, std::forward<Ts>(xs)...);
      CAF_BEFORE_SENDING_SCHEDULED(self, timeout, *element);
      clock.schedule_message(timeout, actor_cast<strong_actor_ptr>(dst),
                             std::move(element));
    }
  } else {
    self->home_system().base_metrics().rejected_messages->inc();
  }
}

} // namespace caf::detail
