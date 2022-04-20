// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "caf/behavior.hpp"
#include "caf/config.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/message_id.hpp"

namespace caf::detail {

struct behavior_stack_mover;

class CAF_CORE_EXPORT behavior_stack {
public:
  friend struct behavior_stack_mover;

  behavior_stack(const behavior_stack&) = delete;
  behavior_stack& operator=(const behavior_stack&) = delete;

  behavior_stack() = default;

  // erases the last (asynchronous) behavior
  void pop_back();

  void clear();

  bool empty() const {
    return elements_.empty();
  }

  behavior& back() {
    CAF_ASSERT(!empty());
    return elements_.back();
  }

  void push_back(behavior&& what) {
    elements_.emplace_back(std::move(what));
  }

  template <class... Ts>
  void emplace_back(Ts&&... xs) {
    elements_.emplace_back(std::forward<Ts>(xs)...);
  }

  void cleanup() {
    erased_elements_.clear();
  }

private:
  std::vector<behavior> elements_;
  std::vector<behavior> erased_elements_;
};

} // namespace caf::detail
