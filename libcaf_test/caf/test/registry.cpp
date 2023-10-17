// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/registry.hpp"

#include "caf/detail/format.hpp"
#include "caf/raise_error.hpp"

namespace caf::test {

registry::~registry() {
  while (head_ != nullptr) {
    auto next = head_->next_;
    delete head_;
    head_ = next;
  }
  while (init_stack_ != nullptr) {
    auto next = init_stack_->next;
    delete init_stack_;
    init_stack_ = next;
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
  if (head_ == nullptr)
    head_ = new_factory;
  else
    tail_->next_ = new_factory;
  tail_ = new_factory;
  return reinterpret_cast<ptrdiff_t>(new_factory);
}

ptrdiff_t registry::add_init_callback(void_function callback) {
  return instance().add(callback);
}

ptrdiff_t registry::add(void_function callback) {
  auto* ptr = new init_callback{init_stack_, callback};
  init_stack_ = ptr;
  return reinterpret_cast<ptrdiff_t>(ptr);
}

void registry::run_init_callbacks() {
  auto* ptr = instance().init_stack_;
  while (ptr != nullptr) {
    ptr->callback();
    ptr = ptr->next;
  }
}

namespace {

registry default_instance;

} // namespace

registry& registry::instance() {
  return default_instance;
}

} // namespace caf::test
