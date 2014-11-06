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

#ifndef NOT_PRIORITIZING_HPP
#define NOT_PRIORITIZING_HPP

#include <deque>
#include <iterator>

#include "caf/mailbox_element.hpp"

#include "caf/policy/priority_policy.hpp"

namespace caf {
namespace policy {

class not_prioritizing {

 public:

  using cache_type = std::deque<unique_mailbox_element_pointer>;

  using cache_iterator = cache_type::iterator;

  template <class Actor>
  unique_mailbox_element_pointer next_message(Actor* self) {
    return unique_mailbox_element_pointer{self->mailbox().try_pop()};
  }

  template <class Actor>
  inline bool has_next_message(Actor* self) {
    return self->mailbox().can_fetch_more();
  }

  inline void push_to_cache(unique_mailbox_element_pointer ptr) {
    m_cache.push_back(std::move(ptr));
  }

  inline cache_iterator cache_begin() {
    return m_cache.begin();
  }

  inline cache_iterator cache_end() {
    return m_cache.end();
  }

  inline void cache_erase(cache_iterator iter) {
    m_cache.erase(iter);
  }

  inline bool cache_empty() const {
    return m_cache.empty();
  }

  inline unique_mailbox_element_pointer cache_take_first() {
    auto tmp = std::move(m_cache.front());
    m_cache.erase(m_cache.begin());
    return std::move(tmp);
  }

  template <class Iterator>
  inline void cache_prepend(Iterator first, Iterator last) {
    auto mi = std::make_move_iterator(first);
    auto me = std::make_move_iterator(last);
    m_cache.insert(m_cache.begin(), mi, me);
  }

 private:

  cache_type m_cache;

};

} // namespace policy
} // namespace caf

#endif // NOT_PRIORITIZING_HPP

