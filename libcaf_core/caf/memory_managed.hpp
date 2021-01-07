// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"

namespace caf {

/// This base enables derived classes to enforce a different
/// allocation strategy than new/delete by providing a virtual
/// protected `request_deletion()` function and non-public destructor.
class CAF_CORE_EXPORT memory_managed {
public:
  /// Default implementations calls `delete this, but can
  /// be overridden in case deletion depends on some condition or
  /// the class doesn't use default new/delete.
  /// @param decremented_rc Indicates whether the caller did reduce the
  ///                       reference of this object before calling this member
  ///                       function. This information is important when
  ///                       implementing a type with support for weak pointers.
  virtual void request_deletion(bool decremented_rc) const noexcept;

protected:
  virtual ~memory_managed();
};

} // namespace caf
