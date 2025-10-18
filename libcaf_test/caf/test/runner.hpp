// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/test/fwd.hpp"

#include "caf/detail/test_export.hpp"

#include <memory>

namespace caf::test {

/// Implements the main loop for running tests.
class CAF_TEST_EXPORT runner {
public:
  virtual ~runner();

  /// Parses the command line arguments and runs the tests.
  virtual int run(int argc, char** argv) = 0;

  /// Creates a new runner.
  static std::unique_ptr<runner> make();

protected:
  void do_run(runnable& what);
};

} // namespace caf::test
