// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/test/block.hpp"

namespace caf::test {

class section : public block {
public:
  using block::block;

  block_type type() const noexcept override;

  section* get_section(int id, std::string_view description,
                       const detail::source_location& loc) override;

  scope commit();
};

} // namespace caf::test
