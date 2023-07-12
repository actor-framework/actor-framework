// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/registry.hpp"

#include "caf/detail/format.hpp"
#include "caf/raise_error.hpp"

namespace caf::test {

registry::~registry() {
  auto ptr = head_;
  while (ptr != nullptr) {
    auto next = ptr->next_;
    delete ptr;
    ptr = next;
  }
}

registry::suites_map registry::suites() {
  suites_map result;
  for (auto* ptr = instance().head_; ptr != nullptr; ptr = ptr->next_) {
    auto& suite = result[ptr->suite_name_];
    if (auto [iter, ok] = suite.emplace(ptr->description_, ptr); !ok) {
      auto msg = detail::format("duplicate test name in suite {}: {}",
                                ptr->suite_name(), ptr->description_);
      CAF_RAISE_ERROR(std::logic_error, msg.c_str());
    }
  }
  return result;
}

ptrdiff_t registry::add(factory* new_factory) {
  if (head_ == nullptr) {
    head_ = new_factory;
    tail_ = head_;
  } else {
    tail_->next_ = new_factory;
    tail_ = new_factory;
  }
  return reinterpret_cast<ptrdiff_t>(new_factory);
}

namespace {

registry default_instance;

} // namespace

registry& registry::instance() {
  return default_instance;
}

} // namespace caf::test
