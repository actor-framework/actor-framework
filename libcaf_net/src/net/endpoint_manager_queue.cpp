// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/endpoint_manager_queue.hpp"

namespace caf::net {

endpoint_manager_queue::element::~element() {
  // nop
}

endpoint_manager_queue::event::event(uri locator, actor listener)
  : element(element_type::event),
    value(resolve_request{std::move(locator), std::move(listener)}) {
  // nop
}

endpoint_manager_queue::event::event(node_id peer, actor_id proxy_id)
  : element(element_type::event), value(new_proxy{peer, proxy_id}) {
  // nop
}

endpoint_manager_queue::event::event(node_id observing_peer,
                                     actor_id local_actor_id, error reason)
  : element(element_type::event),
    value(local_actor_down{observing_peer, local_actor_id, std::move(reason)}) {
  // nop
}

endpoint_manager_queue::event::event(std::string tag, uint64_t id)
  : element(element_type::event), value(timeout{std::move(tag), id}) {
  // nop
}

endpoint_manager_queue::event::~event() {
  // nop
}

size_t endpoint_manager_queue::event::task_size() const noexcept {
  return 1;
}

endpoint_manager_queue::message::message(mailbox_element_ptr msg,
                                         strong_actor_ptr receiver)
  : element(element_type::message),
    msg(std::move(msg)),
    receiver(std::move(receiver)) {
  // nop
}

size_t endpoint_manager_queue::message::task_size() const noexcept {
  return message_policy::task_size(*this);
}

endpoint_manager_queue::message::~message() {
  // nop
}

} // namespace caf::net
