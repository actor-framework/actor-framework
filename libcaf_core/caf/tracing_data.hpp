// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

#include <memory>

namespace caf {

/// Marker interface for application-specific tracing data. This interface
/// enables users to inject application-specific instrumentation into CAF's
/// messaging layer. CAF provides no default implementation for this
/// customization point.
class CAF_CORE_EXPORT tracing_data {
public:
  virtual ~tracing_data();

  /// Writes the content of this object to `sink`.
  virtual bool serialize(serializer& sink) const = 0;

  /// @copydoc serialize
  virtual bool serialize(binary_serializer& sink) const = 0;
};

/// @relates tracing_data
using tracing_data_ptr = std::unique_ptr<tracing_data>;

/// @relates tracing_data
CAF_CORE_EXPORT bool inspect(serializer& sink, const tracing_data_ptr& x);

/// @relates tracing_data
CAF_CORE_EXPORT bool inspect(binary_serializer& sink,
                             const tracing_data_ptr& x);

/// @relates tracing_data
CAF_CORE_EXPORT bool inspect(deserializer& source, tracing_data_ptr& x);

/// @relates tracing_data
CAF_CORE_EXPORT bool inspect(binary_deserializer& source, tracing_data_ptr& x);

} // namespace caf
