/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

#ifndef CAF_POLICY_NESTABLE_INVOKE_HPP
#define CAF_POLICY_NESTABLE_INVOKE_HPP

#include <mutex>
#include <chrono>
#include <condition_variable>

#include "caf/exit_reason.hpp"
#include "caf/mailbox_element.hpp"

#include "caf/detail/sync_request_bouncer.hpp"
#include "caf/detail/single_reader_queue.hpp"

#include "caf/policy/invoke_policy.hpp"

namespace caf {
namespace policy {

class nestable_invoke : public invoke_policy<nestable_invoke> {
 public:
  inline bool hm_should_skip(mailbox_element& node) {
    return node.marked;
  }

  template <class Actor>
  void hm_begin(Actor* self, mailbox_element_ptr& node) {
    node->marked = true;
    node.swap(self->current_element());
  }

  template <class Actor>
  void hm_cleanup(Actor* self, mailbox_element_ptr& node) {
    auto& ref = self->current_element();
    if (ref) {
      ref->marked = false;
    }
    ref.swap(node);
  }
};

} // namespace policy
} // namespace caf

#endif // CAF_POLICY_NESTABLE_INVOKE_HPP
