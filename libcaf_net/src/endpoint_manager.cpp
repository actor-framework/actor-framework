/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/net/endpoint_manager.hpp"

#include "caf/byte.hpp"
#include "caf/intrusive/inbox_result.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/sec.hpp"
#include "caf/send.hpp"

namespace caf {
namespace net {

endpoint_manager::endpoint_manager(socket handle, const multiplexer_ptr& parent,
                                   actor_system& sys)
  : super(handle, parent), sys_(sys), queue_(unit, unit, unit) {
  queue_.try_block();
}

endpoint_manager::~endpoint_manager() {
  // nop
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

void endpoint_manager::resolve(uri locator, actor listener) {
  using intrusive::inbox_result;
  using event_type = endpoint_manager_queue::event;
  auto ptr = new event_type(std::move(locator), std::move(listener));
  if (!enqueue(ptr))
    anon_send(listener, resolve_atom::value,
              make_error(sec::request_receiver_down));
}

void endpoint_manager::enqueue(mailbox_element_ptr msg,
                               strong_actor_ptr receiver,
                               std::vector<byte> payload) {
  using message_type = endpoint_manager_queue::message;
  auto ptr = new message_type(std::move(msg), std::move(receiver),
                              std::move(payload));
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

} // namespace net
} // namespace caf
