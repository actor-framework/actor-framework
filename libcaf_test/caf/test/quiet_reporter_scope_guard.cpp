// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/test/quiet_reporter_scope_guard.hpp"

#include "caf/test/reporter.hpp"

namespace caf::test {

quiet_reporter_scope_guard::quiet_reporter_scope_guard()
  : prev_(&reporter::instance()) {
  scoped_ = reporter::make_quiet();
  scoped_->test_stats(prev_->test_stats());
  reporter::instance(scoped_.get());
}

quiet_reporter_scope_guard::~quiet_reporter_scope_guard() noexcept {
  prev_->test_stats(scoped_->test_stats());
  reporter::instance(prev_);
}

} // namespace caf::test
