// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/test/registry.hpp"

namespace caf::test {

/// Implements the main loop for running tests.
class runner {
public:
  /// Bundles the result of a command line parsing operation.
  struct parse_cli_result {
    /// Stores whether parsing the command line arguments was successful.
    bool ok;
    /// Stores whether a help text was printed.
    bool help_printed;
  };

  runner();

  int run(int argc, char** argv);

private:
  parse_cli_result parse_cli(int argc, char** argv);

  registry::suites_map suites_;
};

} // namespace caf::test
