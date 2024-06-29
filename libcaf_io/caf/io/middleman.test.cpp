// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/io/middleman.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

using namespace caf;
using namespace caf::io;

namespace {

// Regression test for GH-#1900.
// Note: we can't reproduce this in compile time,
// static_assert(std::is_same_v<..snippet.., expected<actor>>) won't work.
TEST("spawn client works") {
  caf::actor_system_config cfg;
  cfg.load<io::middleman>();
  caf::actor_system sys{cfg};
  sys.middleman().spawn_client(
    [](caf::io::broker*, caf::io::connection_handle) {}, "foo", 42);
}

} // namespace
