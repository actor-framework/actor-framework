// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/test/fwd.hpp"

#include "caf/detail/source_location.hpp"
#include "caf/detail/test_export.hpp"
#include "caf/raise_error.hpp"

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
  // -- member types -----------------------------------------------------------

  /// Stores the parameters for a test run.
  using parameter_map = std::map<std::string, std::string>;

  /// The ID of a step in the test.
  using step_id = std::pair<int, size_t>;

  context() = default;

  context(const context&) = delete;

  context& operator=(const context&) = delete;

  ~context();

  // -- member variables -------------------------------------------------------

  /// Stores the current execution stack for the run.
  std::vector<block*> call_stack;

  /// Stores the steps that finished execution this run.
  std::vector<block*> unwind_stack;

  /// Stores all steps that we have reached at least once during the run.
  std::vector<block*> path;

  /// Stores all steps of the test with their run-time ID.
  std::map<step_id, std::unique_ptr<block>> steps;

  /// Stores the parameters for the current run.
  parameter_map parameters;

  /// Stores the current example ID.
  size_t example_id = 0;

  /// Stores the parameters for each example.
  std::vector<parameter_map> example_parameters;

  /// Stores the names of each example.
  std::vector<std::string> example_names;

  // -- properties -------------------------------------------------------------

  /// Returns whether the test is still active. A test is active as long as
  /// no unwinding is in progress.
  bool active() const noexcept;

  /// Checks whether this block has at least one branch that can be executed.
  bool can_run() const noexcept;

  /// Checks whether `ptr` has been activated this run, i.e., whether we can
  /// find it in `unwind_stack`.
  bool activated(block* ptr) const noexcept;

  /// Tries to find `name` in `parameters` and otherwise raises an exception.
  const std::string& parameter(const std::string& name) const;

  // -- mutators ---------------------------------------------------------------

  /// Clears the call and unwind stacks.
  void clear_stacks();

  /// Callback for `block::enter`.
  void on_enter(block* ptr);

  /// Callback for `block::leave`.
  void on_leave(block* ptr);

  /// Returns a new block with the given ID or creates a new one if necessary.
  template <class T>
  T* get(int id, std::string_view description,
         const detail::source_location& loc) {
    auto& result = steps[std::make_pair(id, example_id)];
    if (!result) {
      result = std::make_unique<T>(this, id, description, loc);
    }
    return static_cast<T*>(result.get());
  }

  /// Tries to find the first step that immediately precedes `caller_id` in the
  /// execution path. Returns `nullptr` if no such step exists.
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
