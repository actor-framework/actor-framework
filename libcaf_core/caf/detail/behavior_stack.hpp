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
public:
  friend struct behavior_stack_mover;

  behavior_stack(const behavior_stack&) = delete;
  behavior_stack& operator=(const behavior_stack&) = delete;

  behavior_stack() = default;

  // erases the last (asynchronous) behavior
  void pop_back();

  void clear();

  inline bool empty() const {
    return elements_.empty();
  }

  inline behavior& back() {
    CAF_ASSERT(! empty());
    return elements_.back();
  }

  inline void push_back(behavior&& what) {
    elements_.emplace_back(std::move(what));
  }

  inline void cleanup() {
    erased_elements_.clear();
  }

private:
  std::vector<behavior> elements_;
  std::vector<behavior> erased_elements_;
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_BEHAVIOR_STACK_HPP
