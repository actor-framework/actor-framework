// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/sync_request_bouncer.hpp"

#include "caf/actor.hpp"
#include "caf/config.hpp"
#include "caf/exit_reason.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/message_id.hpp"
#include "caf/sec.hpp"
#include "caf/system_messages.hpp"

namespace caf::detail {

void sync_request_bouncer::operator()(const strong_actor_ptr& sender,
                                      const message_id& mid) const {
  if (sender && mid.is_request())
    sender->enqueue(make_mailbox_element(nullptr, mid.response_id(), rsn),
                    // TODO: this breaks out of the execution unit
                    nullptr);
}

void sync_request_bouncer::operator()(const mailbox_element& e) const {
  (*this)(e.sender, e.mid);
}

} // namespace caf::detail
