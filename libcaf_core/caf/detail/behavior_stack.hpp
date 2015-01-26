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

#ifndef CAF_DETAIL_BEHAVIOR_STACK_HPP
#define CAF_DETAIL_BEHAVIOR_STACK_HPP

#include <vector>
#include <memory>
#include <utility>
#include <algorithm>

#include "caf/optional.hpp"

#include "caf/config.hpp"
#include "caf/behavior.hpp"
#include "caf/message_id.hpp"
#include "caf/mailbox_element.hpp"

namespace caf {
namespace detail {

struct behavior_stack_mover;

class behavior_stack {

  friend struct behavior_stack_mover;

  behavior_stack(const behavior_stack&) = delete;
  behavior_stack& operator=(const behavior_stack&) = delete;

  using element_type = std::pair<behavior, message_id>;

 public:

  behavior_stack() = default;

  // @pre expected_response.valid()
  optional<behavior&> sync_handler(message_id expected_response);

  // erases the last asynchronous message handler
  void pop_async_back();

  void clear();

  // erases the synchronous response handler associated with `rid`
  void erase(message_id rid) {
    erase_if([=](const element_type& e) { return e.second == rid; });
  }

  inline bool empty() const { return m_elements.empty(); }

  inline behavior& back() {
    CAF_REQUIRE(!empty());
    return m_elements.back().first;
  }

  inline message_id back_id() {
    CAF_REQUIRE(!empty());
    return m_elements.back().second;
  }

  inline void push_back(behavior&& what,
              message_id response_id = invalid_message_id) {
    m_elements.emplace_back(std::move(what), response_id);
  }

  inline void cleanup() { m_erased_elements.clear(); }

 private:

  std::vector<element_type> m_elements;
  std::vector<behavior> m_erased_elements;

  // note: checks wheter i points to m_elements.end() before calling erase()
  inline void erase_at(std::vector<element_type>::iterator i) {
    if (i != m_elements.end()) {
      m_erased_elements.emplace_back(std::move(i->first));
      m_elements.erase(i);
    }
  }

  inline void rerase_at(std::vector<element_type>::reverse_iterator i) {
    // base iterator points to the element *after* the correct element
    if (i != m_elements.rend()) erase_at(i.base() - 1);
  }

  template <class UnaryPredicate>
  inline void erase_if(UnaryPredicate p) {
    erase_at(std::find_if(m_elements.begin(), m_elements.end(), p));
  }

  template <class UnaryPredicate>
  inline void rerase_if(UnaryPredicate p) {
    rerase_at(std::find_if(m_elements.rbegin(), m_elements.rend(), p));
  }

};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_BEHAVIOR_STACK_HPP
