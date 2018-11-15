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

#include <chrono>
#include <string>
#include <cstdint>

#include "caf/timespan.hpp"

namespace caf {

/// A portable timestamp with nanosecond resolution anchored at the UNIX epoch.
using timestamp = std::chrono::time_point<std::chrono::system_clock, timespan>;

/// Convenience function for returning a `timestamp` representing
/// the current system time.
timestamp make_timestamp();

/// Prints `x` in ISO 8601 format, e.g., `2018-11-15T06:25:01.462`.
std::string timestamp_to_string(timestamp x);

/// Appends the timestamp `x` in ISO 8601 format, e.g.,
/// `2018-11-15T06:25:01.462`, to `y`.
void append_timestamp_to_string(std::string& x, timestamp y);

} // namespace caf
