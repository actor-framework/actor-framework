/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <memory>

#include "caf/fwd.hpp"

namespace caf {

/// Marker interface for application-specific tracing data. This interface
/// enables users to inject application-specific instrumentation into CAF's
/// messaging layer. CAF provides no default implementation for this
/// customization point.
class tracing_data {
public:
  virtual ~tracing_data();

  /// Writes the content of this object to `sink`.
  virtual error serialize(serializer& sink) const = 0;
};

/// @relates tracing_data
using tracing_data_ptr = std::unique_ptr<tracing_data>;

/// @relates tracing_data
error inspect(serializer& sink, const tracing_data_ptr& x);

/// @relates tracing_data
error inspect(deserializer& source, tracing_data_ptr& x);

} // namespace caf
