// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/mailbox_element.hpp"

#include <memory>

namespace caf {

mailbox_element::mailbox_element(strong_actor_ptr sender, message_id mid,
                                 message payload)
  : sender(std::move(sender)), mid(mid), payload(std::move(payload)) {
  // nop
}

mailbox_element_ptr make_mailbox_element(strong_actor_ptr sender, message_id id,
                                         message payload) {
  return std::make_unique<mailbox_element>(std::move(sender), id,
                                           std::move(payload));
}

} // namespace caf
