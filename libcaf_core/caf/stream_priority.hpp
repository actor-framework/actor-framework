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

#include <string>

namespace caf {

/// Categorizes individual streams.
enum class stream_priority {
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

/// Stores the number of `stream_priority` classes.
static constexpr size_t stream_priorities = 5;

/// @relates stream_priority
std::string to_string(stream_priority x);

} // namespace caf

