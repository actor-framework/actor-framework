// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/test/block_type.hpp"
#include "caf/test/fwd.hpp"

#include "caf/detail/test_export.hpp"

#include <memory>
#include <string_view>

namespace caf::test {

/// A factory for creating runnable test definitions.
class CAF_TEST_EXPORT factory {
public:
  friend class registry;

  factory(std::string_view suite_name, std::string_view description,
          block_type type)
    : suite_name_(suite_name), description_(description), type_(type) {
    // nop
  }

  virtual ~factory();

  /// Returns the name of the suite this factory belongs to.
  std::string_view suite_name() const noexcept {
    return suite_name_;
  }

  /// Creates a new runnable definition for the test.
  virtual std::unique_ptr<runnable> make(context_ptr state) = 0;

protected:
  factory* next_ = nullptr;
  std::string_view suite_name_;
  std::string_view description_;
  block_type type_;
};

} // namespace caf::test
