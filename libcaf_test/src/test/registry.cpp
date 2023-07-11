// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/registry.hpp"

#include "caf/detail/format.hpp"
#include "caf/raise_error.hpp"

namespace caf::test {

registry::suites_map registry::suites() {
  suites_map result;
  for (auto ptr = head_.get(); ptr != nullptr; ptr = ptr->next_.get()) {
    auto& suite = result[ptr->suite_name_];
    if (auto [iter, ok] = suite.emplace(ptr->description_, ptr); !ok) {
      auto msg = detail::format("duplicate test name in suite {}: {}",
                                ptr->suite_name(), ptr->description_);
      CAF_RAISE_ERROR(std::logic_error, msg.c_str());
    }
  }
  return result;
}

std::unique_ptr<factory> registry::head_;

factory* registry::tail_;

} // namespace caf::test
