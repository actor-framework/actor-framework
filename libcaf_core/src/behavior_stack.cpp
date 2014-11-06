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

#include <iterator>

#include "caf/none.hpp"
#include "caf/local_actor.hpp"
#include "caf/detail/behavior_stack.hpp"

using namespace std;

namespace caf {
namespace detail {

struct behavior_stack_mover : iterator<output_iterator_tag, void,
                                       void, void, void> {
 public:
  behavior_stack_mover(behavior_stack* st) : m_stack(st) {}

  behavior_stack_mover& operator=(behavior_stack::element_type&& rval) {
    m_stack->m_erased_elements.emplace_back(move(rval.first));
    return *this;
  }

  behavior_stack_mover& operator*() {
    return *this;
  }

  behavior_stack_mover& operator++() {
    return *this;
  }

  behavior_stack_mover operator++(int) {
    return *this;
  }

 private:
  behavior_stack* m_stack;
};

inline behavior_stack_mover move_iter(behavior_stack* bs) {
  return {bs};
}

optional<behavior&> behavior_stack::sync_handler(message_id expected_response) {
  if (expected_response.valid()) {
    auto e = m_elements.rend();
    auto i = find_if(m_elements.rbegin(), e, [=](element_type& val) {
      return val.second == expected_response;
    });
    if (i != e) {
      return i->first;
    }
  }
  return none;
}

void behavior_stack::pop_async_back() {
  if (m_elements.empty()) {
    return;
  }
  if (!m_elements.back().second.valid()) {
    erase_at(m_elements.end() - 1);
  } else {
    rerase_if([](const element_type& e) { return e.second.valid() == false; });
  }
}

void behavior_stack::clear() {
  if (m_elements.empty() == false) {
    std::move(m_elements.begin(), m_elements.end(), move_iter(this));
    m_elements.clear();
  }
}

} // namespace detail
} // namespace caf
