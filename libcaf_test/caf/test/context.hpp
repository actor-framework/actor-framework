// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/test/block.hpp"
#include "caf/test/fwd.hpp"

#include "caf/detail/source_location.hpp"
#include "caf/detail/test_export.hpp"

#include <map>
#include <memory>
#include <string_view>
#include <vector>

namespace caf::test {

/// Represents the execution context of a test. The context stores all steps of
/// the test and the current execution stack. The context persists across
/// multiple runs of the test in order to select one execution path per run.
class CAF_TEST_EXPORT context {
public:
  /// Returns whether the test is still active. A test is active as long as
  /// no unwinding is in progress.
  bool active() const noexcept {
    return unwind_stack.empty();
  }

  /// Clears the call and unwind stacks.
  void clear_stacks() {
    call_stack.clear();
    unwind_stack.clear();
    path.clear();
  }

  bool can_run();

  void on_enter(block* ptr);

  void on_leave(block* ptr);

  /// Checks whether `ptr` has been activated this run, i.e., whether we can
  /// find it in `unwind_stack`.
  bool activated(block* ptr) const noexcept;

  /// Stores the current execution stack for the run.
  std::vector<block*> call_stack;

  /// Stores the steps that finished execution this run.
  std::vector<block*> unwind_stack;

  /// Stores all steps that we have reached at least once during the run.
  std::vector<block*> path;

  /// Stores all steps of the test with their run-time ID.
  std::map<int, std::unique_ptr<block>> steps;

  template <class T>
  T* get(int id, std::string_view description,
         const detail::source_location& loc) {
    auto& result = steps[id];
    if (!result) {
      result = std::make_unique<T>(this, id, description, loc);
    }
    return static_cast<T*>(result.get());
  }

  template <class T>
  T* find_predecessor(int caller_id) {
    return static_cast<T*>(find_predecessor_block(caller_id, T::type_token));
  }

private:
  block* find_predecessor_block(int caller_id, block_type type);
};

/// A smart pointer to the execution context of a test.
using context_ptr = std::shared_ptr<context>;

} // namespace caf::test
