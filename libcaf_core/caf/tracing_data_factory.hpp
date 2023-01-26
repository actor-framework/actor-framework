// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

namespace caf {

/// Creates instances of @ref tracing_data.
class CAF_CORE_EXPORT tracing_data_factory {
public:
  virtual ~tracing_data_factory();

  /// Deserializes tracing data from `source` and either overrides the content
  /// of `dst` or allocates a new object if `dst` is null.
  /// @returns the result of `source`.
  virtual bool
  deserialize(deserializer& source, std::unique_ptr<tracing_data>& dst) const
    = 0;

  /// @copydoc deserialize
  virtual bool deserialize(binary_deserializer& source,
                           std::unique_ptr<tracing_data>& dst) const
    = 0;
};

} // namespace caf
