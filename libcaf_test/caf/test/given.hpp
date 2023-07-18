// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/test/block.hpp"

#include "caf/detail/test_export.hpp"

namespace caf::test {

/// Represents a `GIVEN` block.
class CAF_TEST_EXPORT given : public block {
public:
  using block::block;

  block_type type() const noexcept override;

  when* get_when(int id, std::string_view description,
                 const detail::source_location& loc) override;

  and_when* get_and_when(int id, std::string_view description,
                         const detail::source_location& loc) override;

  scope commit();
};

} // namespace caf::test
