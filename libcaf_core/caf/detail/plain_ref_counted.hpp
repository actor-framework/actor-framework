// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"

#include <cstddef>

namespace caf::detail {

/// Base class for reference counted objects with an plain (i.e., thread-unsafe)
/// reference count.
class CAF_CORE_EXPORT plain_ref_counted {
public:
  virtual ~plain_ref_counted();

  plain_ref_counted();
  plain_ref_counted(const plain_ref_counted&);
  plain_ref_counted& operator=(const plain_ref_counted&);

  /// Increases reference count by one.
  void ref() const noexcept {
    ++rc_;
  }

  /// Decreases reference count by one and calls `request_deletion`
  /// when it drops to zero.
  void deref() const noexcept {
    if (rc_ > 1)
      --rc_;
    else
      delete this;
  }

  /// Queries whether there is exactly one reference.
  bool unique() const noexcept {
    return rc_ == 1;
  }

  /// Queries the current reference count for this object.
  size_t get_reference_count() const noexcept {
    return rc_;
  }

protected:
  mutable size_t rc_;
};

} // namespace caf::detail
