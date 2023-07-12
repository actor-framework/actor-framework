// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/test_export.hpp"
#include "caf/test/block.hpp"
#include "caf/test/block_type.hpp"

namespace caf::test {

class CAF_TEST_EXPORT when : public block {
public:
  using block::block;

  static constexpr block_type type_token = block_type::when;

  then* get_then(int id, std::string_view description,
                 const detail::source_location& loc) override;

  and_then* get_and_then(int id, std::string_view description,
                         const detail::source_location& loc) override;

  block_type type() const noexcept override;

  scope commit();
};

} // namespace caf::test
