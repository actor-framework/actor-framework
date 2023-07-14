// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/test_export.hpp"
#include "caf/settings.hpp"
#include "caf/test/registry.hpp"

#include <optional>
#include <regex>

namespace caf::test {

/// Implements the main loop for running tests.
class CAF_TEST_EXPORT runner {
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
  static std::optional<std::regex> to_regex(std::string_view regex_string);

  registry::suites_map suites_;
  caf::settings cfg_;
};

} // namespace caf::test
