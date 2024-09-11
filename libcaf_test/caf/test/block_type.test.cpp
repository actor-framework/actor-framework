// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/test/block_type.hpp"

#include "caf/test/test.hpp"

using caf::test::block_type;

TEST("is_extension checks whether a block type needs a predecessor") {
  using caf::test::is_extension;
  SECTION("is_extension is true for all AND_* types and BUT") {
    check(is_extension(block_type::and_given));
    check(is_extension(block_type::and_when));
    check(is_extension(block_type::and_then));
    check(is_extension(block_type::but));
  }
  SECTION("is_extension is false for regular types") {
    check(!is_extension(block_type::test));
    check(!is_extension(block_type::section));
    check(!is_extension(block_type::outline));
    check(!is_extension(block_type::scenario));
    check(!is_extension(block_type::given));
    check(!is_extension(block_type::when));
    check(!is_extension(block_type::then));
  }
}
