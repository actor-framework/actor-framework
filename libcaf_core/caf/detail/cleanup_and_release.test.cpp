// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/cleanup_and_release.hpp"

#include "caf/test/test.hpp"

#include "caf/make_counted.hpp"
#include "caf/ref_counted.hpp"
#include "caf/resumable.hpp"
#include "caf/scheduler.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

using namespace caf;
using namespace caf::detail;

namespace {

struct mock_base : resumable, ref_counted {
  explicit mock_base(std::shared_ptr<std::atomic<bool>> disposed_flag)
    : disposed_flag(std::move(disposed_flag)) {
  }

  void ref_resumable() const noexcept final {
    ref();
  }

  void deref_resumable() const noexcept final {
    deref();
  }

  std::shared_ptr<std::atomic<bool>> disposed_flag;
};

struct single_mock : mock_base {
  using mock_base::mock_base;

  void resume(scheduler*, uint64_t event_id) override {
    if (event_id == resumable::dispose_event_id)
      disposed_flag->store(true);
  }
};

struct nested_mock : mock_base {
  using child_ptr = intrusive_ptr<mock_base>;

  nested_mock(std::shared_ptr<std::atomic<bool>> disposed_flag,
              std::vector<child_ptr> children)
    : mock_base(std::move(disposed_flag)), children(std::move(children)) {
  }

  void resume(scheduler* ctx, uint64_t event_id) override {
    if (event_id == resumable::dispose_event_id) {
      disposed_flag->store(true);
      for (auto& child : children) {
        intrusive_ptr_add_ref(child.get());
        ctx->delay(child.get(), resumable::dispose_event_id);
      }
      return;
    }
  }

  std::vector<child_ptr> children;
};

} // namespace

TEST("cleanup_and_release resumes a single resumable with dispose_event_id") {
  auto disposed = std::make_shared<std::atomic<bool>>(false);
  auto p = make_counted<single_mock>(disposed);
  cleanup_and_release(p.release());
  check(disposed->load());
}

TEST("cleanup_and_release recursively disposes nested resumables") {
  // Create our flags.
  auto parent_disposed = std::make_shared<std::atomic<bool>>(false);
  auto child1_disposed = std::make_shared<std::atomic<bool>>(false);
  auto child2_disposed = std::make_shared<std::atomic<bool>>(false);
  // Create the resumables.
  std::vector<nested_mock::child_ptr> children;
  children.emplace_back(make_counted<single_mock>(child1_disposed));
  children.emplace_back(make_counted<single_mock>(child2_disposed));
  auto parent = make_counted<nested_mock>(parent_disposed, std::move(children));
  // Clean up the parent and its children.
  cleanup_and_release(parent.release());
  // Check the flags.
  check(parent_disposed->load());
  check(child1_disposed->load());
  check(child2_disposed->load());
}
