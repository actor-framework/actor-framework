// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/send.hpp"

namespace caf::detail {

disposable
do_delayed_anon_send(strong_actor_ptr&& receiver, message_priority priority,
                     actor_clock::duration_type timeout, message&& msg) {
  if (receiver == nullptr)
    return {};
  auto& clock = receiver->home_system->clock();
  auto item = make_mailbox_element(nullptr, make_message_id(priority),
                                   std::move(msg));
  return clock.schedule_message(clock.now() + timeout, std::move(receiver),
                                std::move(item));
}

disposable
do_scheduled_anon_send(strong_actor_ptr&& receiver, message_priority priority,
                       actor_clock::time_point timeout, message&& msg) {
  if (receiver == nullptr)
    return {};
  auto& clock = receiver->home_system->clock();
  auto item = make_mailbox_element(nullptr, make_message_id(priority),
                                   std::move(msg));
  return clock.schedule_message(timeout, std::move(receiver), std::move(item));
}

} // namespace caf::detail
