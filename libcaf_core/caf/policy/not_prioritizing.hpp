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

#ifndef CAF_POLICY_NOT_PRIORITIZING_HPP
#define CAF_POLICY_NOT_PRIORITIZING_HPP

#include <deque>
#include <iterator>

#include "caf/mailbox_element.hpp"

#include "caf/policy/invoke_policy.hpp"
#include "caf/policy/priority_policy.hpp"

namespace caf {
namespace policy {

class not_prioritizing {
 public:
  template <class Actor>
  mailbox_element_ptr next_message(Actor* self) {
    return mailbox_element_ptr{self->mailbox().try_pop()};
  }

  template <class Actor>
  bool has_next_message(Actor* self) {
    return self->mailbox().can_fetch_more();
  }

  template <class Actor>
  void push_to_cache(Actor* self, mailbox_element_ptr ptr) {
    self->mailbox().cache().push_second_back(ptr.release());
  }

  template <class Actor, class... Ts>
  bool invoke_from_cache(Actor* self, Ts&... args) {
    auto& cache = self->mailbox().cache();
    auto i = cache.second_begin();
    auto e = cache.second_end();
    CAF_LOG_DEBUG(std::distance(i, e) << " elements in cache");
    while (i != e) {
      switch (self->invoke_message(*i, args...)) {
        case im_dropped:
          i = cache.erase(i);
          break;
        case im_success:
          cache.erase(i);
          return true;
        default:
          ++i;
          break;
      }
    }
    return false;
  }
};

} // namespace policy
} // namespace caf

#endif // CAF_POLICY_NOT_PRIORITIZING_HPP
