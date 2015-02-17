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
  typename Actor::mailbox_type::unique_pointer next_message(Actor* self) {
    typename Actor::mailbox_type::unique_pointer result;
    result.reset(self->mailbox().try_pop());
    return result;
  }

  template <class Actor>
  bool has_next_message(Actor* self) {
    return self->mailbox().can_fetch_more();
  }

  template <class Actor>
  void push_to_cache(Actor* self,
                     typename Actor::mailbox_type::unique_pointer ptr) {
    self->mailbox().cache().push_second_back(ptr.release());
  }

  template <class Actor, class... Vs>
  bool invoke_from_cache(Actor* self, Vs&... args) {
    auto& cache = self->mailbox().cache();
    auto i = cache.second_begin();
    auto e = cache.second_end();
    CAF_LOG_DEBUG(std::distance(i, e) << " elements in cache");
    return cache.invoke(self, i, e, args...);
  }
};

} // namespace policy
} // namespace caf

#endif // CAF_POLICY_NOT_PRIORITIZING_HPP
