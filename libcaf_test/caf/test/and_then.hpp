// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/test/block.hpp"

namespace caf::test {

/// Represents an `AND_THEN` block.
class and_then : public block {
public:
  using block::block;

  block_type type() const noexcept override;

  scope commit();
};

} // namespace caf::test
