// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/io/middleman.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

using namespace caf;
using namespace caf::io;

namespace {

TEST("GH-1900 regression") {
  // Note: we don't need to actually run `spawn_client`, we only check for a
  // compiler error. We shouldn't actually run this code, since it will assume
  // 42 is a socket descriptor.
  auto fn = [] {
    caf::actor_system_config cfg;
    cfg.load<io::middleman>();
    caf::actor_system sys{cfg};
    sys.middleman().spawn_client(
      [](caf::io::broker*, caf::io::connection_handle) {}, "foo", 42);
  };
  check(std::is_same_v<decltype(fn), decltype(fn)>);
}

} // namespace
