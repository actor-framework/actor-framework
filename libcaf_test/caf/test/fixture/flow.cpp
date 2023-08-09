// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/fixture/flow.hpp"

namespace caf::test::fixture {

void flow::run_flows() {
  // TODO: the scoped_coordinator is not the right tool for this job. We need a
  //       custom coordinator that allows us to control timeouts. For now, this
  //       is only good enough to get tests running that have no notion of time.
  coordinator_->run_some();
}

} // namespace caf::test::fixture
