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

#include "caf/io/basp/message_queue.hpp"

#include <iterator>

namespace caf {
namespace io {
namespace basp {

message_queue::message_queue() : next_id(0), next_undelivered(0) {
  // nop
}

void message_queue::push(execution_unit* ctx, uint64_t id,
                         strong_actor_ptr receiver,
                         mailbox_element_ptr content) {
  std::unique_lock<std::mutex> guard{lock};
  CAF_ASSERT(id >= next_undelivered);
  CAF_ASSERT(id < next_id);
  auto first = pending.begin();
  auto last = pending.end();
  if (id == next_undelivered) {
    // Dispatch current head.
    if (receiver != nullptr)
      receiver->enqueue(std::move(content), ctx);
    // Check whether we can deliver more.
    if (first == last || first->id != next_undelivered + 1) {
      ++next_undelivered;
      CAF_ASSERT(next_undelivered <= next_id);
      return;
    }
    // We look for the last element that we cannot ship right away.
    auto pred = [](const actor_msg& x, const actor_msg& y) {
      return x.id + 1 > y.id;
    };
    auto last_hit = std::adjacent_find(first, last, pred);
    CAF_ASSERT(last_hit != first);
    auto new_last = last_hit == last ? last : last_hit + 1;
    for (auto i = first; i != new_last; ++i)
      if (i->receiver != nullptr)
        i->receiver->enqueue(std::move(i->content), ctx);
    next_undelivered += static_cast<size_t>(std::distance(first, new_last)) + 1;
    pending.erase(first, new_last);
    CAF_ASSERT(next_undelivered <= next_id);
    return;
  }
  // Get the insertion point.
  auto pred = [&](const actor_msg& x) {
    return x.id >= id;
  };
  pending.emplace(std::find_if(first, last, pred),
                  actor_msg{id, std::move(receiver), std::move(content)});
}

void message_queue::drop(execution_unit* ctx, uint64_t id) {
  push(ctx, id, nullptr, nullptr);
}

uint64_t message_queue::new_id() {
  std::unique_lock<std::mutex> guard{lock};
  return next_id++;
}

} // namespace basp
} // namespace io
} // namespace caf
