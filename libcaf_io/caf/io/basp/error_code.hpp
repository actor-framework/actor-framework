/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#ifndef CAF_IO__HPP
#define CAF_IO__HPP

#include <cstdint>

namespace caf {
namespace io {
namespace basp {

/// @addtogroup BASP

/// Describes an error during forwarding of BASP messages.
enum class error_code : uint64_t {
  /// Indicates that a forwarding node had no route
  /// to the destination.
  no_route_to_destination = 0x01,

  /// Indicates that a forwarding node detected
  /// a loop in the forwarding path.
  loop_detected = 0x02
};

/// @relates error_code
constexpr const char* to_string(error_code x) {
  return x == error_code::no_route_to_destination ? "no_route_to_destination"
                                                  : "loop_detected";
}

/// @}

} // namespace basp
} // namespace io
} // namespace caf

#endif // CAF_IO__HPP

