// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/timespan.hpp"

#include <chrono>
#include <cstdint>
#include <string>

namespace caf {

/// A portable timestamp with nanosecond resolution anchored at the UNIX epoch.
using timestamp = std::chrono::time_point<std::chrono::system_clock, timespan>;

/// Convenience function for returning a `timestamp` representing
/// the current system time.
CAF_CORE_EXPORT timestamp make_timestamp();

/// Prints `x` in ISO 8601 format, e.g., `2018-11-15T06:25:01.462`.
CAF_CORE_EXPORT std::string timestamp_to_string(timestamp x);

/// Converts an ISO 8601 formatted timestamp into its native representation.
CAF_CORE_EXPORT expected<timestamp> timestamp_from_string(std::string_view str);

/// Appends the timestamp `x` in ISO 8601 format, e.g.,
/// `2018-11-15T06:25:01.462`, to `y`.
CAF_CORE_EXPORT void append_timestamp_to_string(std::string& x, timestamp y);

} // namespace caf
