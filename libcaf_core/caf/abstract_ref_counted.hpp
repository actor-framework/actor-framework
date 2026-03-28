// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"

namespace caf {

/// Interface for types that can be used with @ref intrusive_ptr.
/// @relates intrusive_ptr
class CAF_CORE_EXPORT abstract_ref_counted {
public:
  virtual ~abstract_ref_counted() noexcept;

  virtual void ref() const noexcept = 0;

  virtual void deref() const noexcept = 0;
};

} // namespace caf
