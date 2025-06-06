// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/test/fwd.hpp"

#include "caf/detail/source_location.hpp"
#include "caf/detail/test_export.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace caf::test {

/// Represents a block of test logic. Blocks can be nested to form a tree-like
/// structure.
class CAF_TEST_EXPORT block {
public:
  // Note: the context owns the block. Hence, we can use a raw pointer for
  //       pointing back to the parent object here.
  block(context* ctx, int id, std::string_view description,
        const detail::source_location& loc);

  virtual ~block();

  /// Returns the type of this block.
  virtual block_type type() const noexcept = 0;

  /// Returns the user-defined description of this block.
  std::string_view description() const noexcept {
    return description_;
  }

  /// Returns the parameters names from the description of this block.
  const std::vector<std::string>& parameter_names() const noexcept {
    return parameter_names_;
  }

  /// Returns the source location of this block.
  const detail::source_location& location() const noexcept {
    return loc_;
  }

  /// Called at scope entry.
  void enter();

  /// Called from the root block to clean up a branch of the test.
  void leave();

  /// Customization point for performing sanity checks before leaving the block.
  virtual void on_leave();

  /// Checks whether this block can run. This is used to skip blocks that were
  /// executed in a previous run or are scheduled to run in a future run.
  bool can_run() const noexcept;

  /// Checks whether this block is active. A block is active if it is currently
  /// executed.
  bool active() const noexcept {
    return active_;
  }

  template <class T>
  T* get_nested(int id, std::string_view description,
                const detail::source_location& loc) {
    auto& result = get_nested_or_construct(id);
    if (!result) {
      result = std::make_unique<T>(ctx_, id, description, loc);
      nested_.push_back(result.get());
    } else if (std::find(nested_.begin(), nested_.end(), result.get())
               == nested_.end()) {
      nested_.push_back(result.get());
    }
    return static_cast<T*>(result.get());
  }

  virtual section* get_section(int id, std::string_view description,
                               const detail::source_location& loc
                               = detail::source_location::current());

  virtual given* get_given(int id, std::string_view description,
                           const detail::source_location& loc
                           = detail::source_location::current());

  virtual and_given* get_and_given(int id, std::string_view description,
                                   const detail::source_location& loc
                                   = detail::source_location::current());

  virtual when* get_when(int id, std::string_view description,
                         const detail::source_location& loc
                         = detail::source_location::current());

  virtual and_when* get_and_when(int id, std::string_view description,
                                 const detail::source_location& loc
                                 = detail::source_location::current());

  virtual then* get_then(int id, std::string_view description,
                         const detail::source_location& loc
                         = detail::source_location::current());

  virtual and_then* get_and_then(int id, std::string_view description,
                                 const detail::source_location& loc
                                 = detail::source_location::current());

  virtual but* get_but(int id, std::string_view description,
                       const detail::source_location& loc
                       = detail::source_location::current());

protected:
  std::unique_ptr<block>& get_nested_or_construct(int id);

  context* ctx_;
  int id_ = 0;
  std::string_view raw_description_;
  std::string description_;
  bool active_ = false;
  bool executed_ = false;
  std::vector<block*> nested_;
  detail::source_location loc_;
  std::vector<std::string> parameter_names_;

private:
  void lazy_init();
};

} // namespace caf::test
