// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include <iterator>

#include "caf/detail/behavior_stack.hpp"
#include "caf/local_actor.hpp"
#include "caf/none.hpp"

namespace caf::detail {

void behavior_stack::pop_back() {
  CAF_ASSERT(!elements_.empty());
  erased_elements_.push_back(std::move(elements_.back()));
  elements_.pop_back();
}

void behavior_stack::clear() {
  if (!elements_.empty()) {
    if (erased_elements_.empty()) {
      elements_.swap(erased_elements_);
    } else {
      std::move(elements_.begin(), elements_.end(),
                std::back_inserter(erased_elements_));
      elements_.clear();
    }
  }
}

} // namespace caf::detail
