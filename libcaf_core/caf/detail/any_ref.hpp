// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

namespace caf::detail {

/// Polymorphic, type-erasure reference. Implementation detail for `any_view`
/// and `const_any_view`.
class CAF_CORE_EXPORT any_ref {
public:
  virtual ~any_ref() noexcept;

  virtual type_id_t type_id() const noexcept = 0;

  virtual void* get() noexcept = 0;

  virtual const void* get() const noexcept = 0;
};

} // namespace caf::detail
