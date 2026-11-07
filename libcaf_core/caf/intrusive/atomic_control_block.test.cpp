// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/intrusive/atomic_control_block.hpp"

#include "caf/test/test.hpp"

#include <atomic>
#include <memory>

using namespace caf;

namespace {

using atomic_bool_ptr = std::shared_ptr<std::atomic<bool>>;

class test_control_block final : public intrusive::atomic_control_block {
public:
  test_control_block(atomic_bool_ptr destroyed, atomic_bool_ptr deleted)
    : destroyed_(destroyed), deleted_(deleted) {
    // nop
  }

  void destroy_managed() noexcept override {
    *destroyed_ = true;
  }

  void delete_this() noexcept override {
    *deleted_ = true;
    delete this;
  }

  atomic_bool_ptr destroyed_;
  atomic_bool_ptr deleted_;
};

} // namespace

TEST("atomic control blocks keep weak references after object destruction") {
  auto destroyed = std::make_shared<std::atomic<bool>>(false);
  auto deleted = std::make_shared<std::atomic<bool>>(false);
  auto* uut = new test_control_block(destroyed, deleted);
  check_eq(uut->strong_reference_count(), 1u);
  check_eq(uut->weak_reference_count(), 1u);
  uut->ref_weak();
  check(uut->upgrade_weak());
  check_eq(uut->strong_reference_count(), 2u);
  check_eq(uut->weak_reference_count(), 2u);
  uut->deref();
  check(!*destroyed);
  uut->deref();
  check(*destroyed);
  check(!*deleted);
  check(!uut->upgrade_weak());
  check_eq(uut->weak_reference_count(), 1u);
  uut->deref_weak();
  check(*deleted);
}
