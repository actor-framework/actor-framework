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

#ifndef CAF_POLICY_PRIORITIZING_HPP
#define CAF_POLICY_PRIORITIZING_HPP

#include <list>

#include "caf/mailbox_element.hpp"
#include "caf/message_priority.hpp"

#include "caf/policy/not_prioritizing.hpp"

#include "caf/detail/single_reader_queue.hpp"
#include "caf/detail/sync_request_bouncer.hpp"

namespace caf {
namespace policy {

// this policy partitions the mailbox into four segments:
// <-------- !was_skipped --------> | <--------  was_skipped -------->
// <-- high prio --><-- low prio -->|<-- high prio --><-- low prio -->
class prioritizing {
 public:
  using pointer = mailbox_element*;
  using mailbox_type = detail::single_reader_queue<mailbox_element,
                                                   detail::disposer>;
  using cache_type = mailbox_type::cache_type;
  using iterator = cache_type::iterator;

  template <class Actor>
  typename Actor::mailbox_type::unique_pointer next_message(Actor* self) {
    auto& cache = self->mailbox().cache();
    auto i = cache.first_begin();
    auto e = cache.first_end();
    if (i == e || !i->is_high_priority()) {
      // insert points for high priority
      auto hp_pos = i;
      // read whole mailbox at once
      auto tmp = self->mailbox().try_pop();
      while (tmp) {
        cache.insert(tmp->is_high_priority() ? hp_pos : e, tmp);
        // adjust high priority insert point on first low prio element insert
        if (hp_pos == e && !tmp->is_high_priority()) {
          --hp_pos;
        }
        tmp = self->mailbox().try_pop();
      }
    }
    typename Actor::mailbox_type::unique_pointer result;
    if (!cache.first_empty()) {
      result.reset(cache.take_first_front());
    }
    return result;
  }

  template <class Actor>
  bool has_next_message(Actor* self) {
    auto& mbox = self->mailbox();
    auto& cache = mbox.cache();
    return !cache.first_empty() || mbox.can_fetch_more();
  }

  template <class Actor>
  void push_to_cache(Actor* self, unique_mailbox_element_pointer ptr) {
    auto high_prio = [](const mailbox_element& val) {
      return val.is_high_priority();
    };
    auto& cache = self->mailbox().cache();
    auto e = cache.second_end();
    auto i = ptr->is_high_priority()
             ? std::partition_point(cache.second_begin(), e, high_prio)
             : e;
    cache.insert(i, ptr.release());
  }

  template <class Actor, class... Ts>
  bool invoke_from_cache(Actor* self, Ts&... args) {
    // same as in not prioritzing policy
    not_prioritizing np;
    return np.invoke_from_cache(self, args...);
  }
};

} // namespace policy
} // namespace caf

#endif // CAF_POLICY_PRIORITIZING_HPP
