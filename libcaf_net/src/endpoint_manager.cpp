// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/endpoint_manager.hpp"

#include "caf/intrusive/inbox_result.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/sec.hpp"
#include "caf/send.hpp"

namespace caf::net {

// -- constructors, destructors, and assignment operators ----------------------

endpoint_manager::endpoint_manager(socket handle, const multiplexer_ptr& parent,
                                   actor_system& sys)
  : super(handle, parent), sys_(sys), queue_(unit, unit, unit) {
  queue_.try_block();
}

endpoint_manager::~endpoint_manager() {
  // nop
}

// -- properties ---------------------------------------------------------------

const actor_system_config& endpoint_manager::config() const noexcept {
  return sys_.config();
}

// -- queue access -------------------------------------------------------------

bool endpoint_manager::at_end_of_message_queue() {
  return queue_.empty() && queue_.try_block();
}

endpoint_manager_queue::message_ptr endpoint_manager::next_message() {
  if (queue_.blocked())
    return nullptr;
  queue_.fetch_more();
  auto& q = std::get<1>(queue_.queue().queues());
  auto ts = q.next_task_size();
  if (ts == 0)
    return nullptr;
  q.inc_deficit(ts);
  auto result = q.next();
  if (queue_.empty())
    queue_.try_block();
  return result;
}

// -- event management ---------------------------------------------------------

void endpoint_manager::resolve(uri locator, actor listener) {
  using intrusive::inbox_result;
  using event_type = endpoint_manager_queue::event;
  auto ptr = new event_type(std::move(locator), listener);
  if (!enqueue(ptr))
    anon_send(listener, resolve_atom_v, make_error(sec::request_receiver_down));
}

void endpoint_manager::enqueue(mailbox_element_ptr msg,
                               strong_actor_ptr receiver) {
  using message_type = endpoint_manager_queue::message;
  auto ptr = new message_type(std::move(msg), std::move(receiver));
  enqueue(ptr);
}

bool endpoint_manager::enqueue(endpoint_manager_queue::element* ptr) {
  switch (queue_.push_back(ptr)) {
    case intrusive::inbox_result::success:
      return true;
    case intrusive::inbox_result::unblocked_reader: {
      auto mpx = parent_.lock();
      if (mpx) {
        mpx->register_writing(this);
        return true;
      }
      return false;
    }
    default:
      return false;
  }
}

} // namespace caf::net
