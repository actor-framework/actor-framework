// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/test/block.hpp"
#include "caf/test/block_type.hpp"

#include "caf/detail/test_export.hpp"

namespace caf::test {

/// Represents a `THEN` block.
class CAF_TEST_EXPORT then : public block {
public:
  using block::block;

  static constexpr block_type type_token = block_type::then;

  block_type type() const noexcept override;

  scope commit();
};

} // namespace caf::test
