// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/sync_request_bouncer.hpp"

#include "caf/actor.hpp"
#include "caf/actor_cast.hpp"
#include "caf/config.hpp"
#include "caf/exit_reason.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/message_id.hpp"
#include "caf/sec.hpp"
#include "caf/system_messages.hpp"

namespace caf::detail {

sync_request_bouncer::sync_request_bouncer(error r) : rsn(std::move(r)) {
  // nop
}

void sync_request_bouncer::operator()(const strong_actor_ptr& sender,
                                      const message_id& mid) const {
  if (sender && mid.is_request())
    sender->enqueue(nullptr, mid.response_id(), make_message(rsn),
                    // TODO: this breaks out of the execution unit
                    nullptr);
}

intrusive::task_result
sync_request_bouncer::operator()(const mailbox_element& e) const {
  (*this)(e.sender, e.mid);
  return intrusive::task_result::resume;
}

} // namespace caf::detail
