/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_TIMESTAMP_HPP
#define CAF_TIMESTAMP_HPP

#include <chrono>
#include <string>
#include <cstdint>

namespace caf {

/// A portable timestamp with nanosecond resolution anchored at the UNIX epoch.
using timestamp = std::chrono::time_point<
  std::chrono::system_clock,
  std::chrono::duration<int64_t, std::nano>
>;

/// Convenience function for returning a `timestamp` representing
/// the current system time.
timestamp make_timestamp();

/// Converts the time-since-epoch of `x` to a `string`.
std::string timestamp_to_string(const timestamp& x);

/// Appends the time-since-epoch of `y` to `x`.
void append_timestamp_to_string(std::string& x, const timestamp& y);

} // namespace caf

#endif // CAF_TIMESTAMP_HPP
