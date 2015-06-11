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

void actor_companion::disconnect(std::uint32_t rsn) {
  enqueue_handler tmp;
  { // lifetime scope of guard
    std::lock_guard<lock_type> guard(lock_);
    on_enqueue_.swap(tmp);
  }
  cleanup(rsn);
}

void actor_companion::on_enqueue(enqueue_handler handler) {
  std::lock_guard<lock_type> guard(lock_);
  on_enqueue_ = std::move(handler);
}

void actor_companion::enqueue(mailbox_element_ptr ptr, execution_unit*) {
  shared_lock<lock_type> guard(lock_);
  if (on_enqueue_) {
    on_enqueue_(std::move(ptr));
  }
}

void actor_companion::enqueue(const actor_addr& sender, message_id mid,
                              message content, execution_unit* eu) {
  using detail::memory;
  auto ptr = mailbox_element::make(sender, mid, std::move(content));
  enqueue(std::move(ptr), eu);
}

void actor_companion::initialize() {
  // nop
}

} // namespace caf
