// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/test_export.hpp"

#include <memory>

namespace caf::test {

class reporter;

/// Installs `reporter::make_quiet()` as `reporter::instance` for the scope.
class CAF_TEST_EXPORT quiet_reporter_scope_guard {
public:
  quiet_reporter_scope_guard();

  ~quiet_reporter_scope_guard() noexcept;

  quiet_reporter_scope_guard(const quiet_reporter_scope_guard&) = delete;
  quiet_reporter_scope_guard& operator=(const quiet_reporter_scope_guard&)
    = delete;
  quiet_reporter_scope_guard(quiet_reporter_scope_guard&&) = delete;
  quiet_reporter_scope_guard& operator=(quiet_reporter_scope_guard&&) = delete;

private:
  reporter* prev_;
  std::unique_ptr<reporter> scoped_;
};

} // namespace caf::test
