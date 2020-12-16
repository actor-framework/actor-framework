/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
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

#include <cstdint>
#include <string>
#include <type_traits>

#include "caf/default_enum_inspect.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

namespace caf {

/// Categorizes individual streams.
enum class stream_priority : uint8_t {
  /// Denotes soft-realtime traffic.
  very_high,
  /// Denotes time-sensitive traffic.
  high,
  /// Denotes traffic with moderate timing requirements.
  normal,
  /// Denotes uncritical traffic without timing requirements.
  low,
  /// Denotes best-effort traffic.
  very_low
};

/// @relates stream_priority
CAF_CORE_EXPORT std::string to_string(stream_priority x);

/// @relates stream_priority
CAF_CORE_EXPORT bool from_string(string_view, stream_priority&);

/// @relates stream_priority
CAF_CORE_EXPORT bool from_integer(std::underlying_type_t<stream_priority>,
                                  stream_priority&);

/// @relates stream_priority
template <class Inspector>
bool inspect(Inspector& f, stream_priority& x) {
  return default_enum_inspect(f, x);
}

} // namespace caf
