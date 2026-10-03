// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/none.hpp"

#include "caf/test/test.hpp"

using namespace caf;

TEST("to_string") {
  check_eq(to_string(none), "none");
}

TEST("comparison") {
  check_eq(none, none);
}
