/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/locks.hpp"
#include "caf/actor_companion.hpp"

namespace caf {

void actor_companion::disconnect(exit_reason rsn) {
  enqueue_handler tmp;
  { // lifetime scope of guard
    std::lock_guard<lock_type> guard(lock_);
    on_enqueue_.swap(tmp);
  }
  cleanup(rsn, context());
}

void actor_companion::on_enqueue(enqueue_handler handler) {
  std::lock_guard<lock_type> guard(lock_);
  on_enqueue_ = std::move(handler);
}

void actor_companion::enqueue(mailbox_element_ptr ptr, execution_unit*) {
  CAF_ASSERT(ptr);
  shared_lock<lock_type> guard(lock_);
  if (on_enqueue_)
    on_enqueue_(std::move(ptr));
}

void actor_companion::enqueue(strong_actor_ptr src, message_id mid,
                              message content, execution_unit* eu) {
  auto ptr = make_mailbox_element(std::move(src), mid, {}, std::move(content));
  enqueue(std::move(ptr), eu);
}

} // namespace caf
