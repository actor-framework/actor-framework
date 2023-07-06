// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/test/block.hpp"
#include "caf/test/block_type.hpp"

namespace caf::test {

/// Represents a `THEN` block.
class then : public block {
public:
  using block::block;

  static constexpr block_type type_token = block_type::then;

  block_type type() const noexcept override;

  scope commit();
};

} // namespace caf::test
