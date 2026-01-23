// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/net_export.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/ref_counted.hpp"

namespace caf::detail {

/// Abstract interface for tracking connection lifetime. Implementations allow
/// users to query whether the underlying connection has been orphaned, i.e.,
/// the socket manager that created the connection has shut down.
class CAF_NET_EXPORT connection_guard : public ref_counted {
public:
  ~connection_guard() override;

  /// Returns `true` if the socket manager that created this guard has shut
  /// down, `false` otherwise.
  [[nodiscard]] virtual bool orphaned() const noexcept = 0;

  /// Sets the orphaned flag to `true`.
  virtual void set_orphaned() noexcept = 0;
};

using connection_guard_ptr = intrusive_ptr<connection_guard>;

} // namespace caf::detail
