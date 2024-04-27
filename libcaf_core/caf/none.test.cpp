// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/none.hpp"

#include "caf/test/test.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"

using namespace caf;

namespace {

TEST("none is serializable") {
  check_eq(to_string(none), "none");
}

TEST("none is comparable") {
  check(!none);
  check_eq(none.compare(none), 0);
}

} // namespace
