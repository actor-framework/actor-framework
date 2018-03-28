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

#include "caf/actor_system.hpp"

#include "caf/io/middleman.hpp"

namespace caf {
namespace io {

/// Unpublishes `whom` by closing `port` or all assigned ports if `port == 0`.
/// @param whom Actor that should be unpublished at `port`.
/// @param port TCP port.
template <class Handle>
expected<void> unpublish(const Handle& whom, uint16_t port = 0) {
  if (!whom)
    return sec::invalid_argument;
  return whom.home_system().middleman().unpublish(whom, port);
}

} // namespace io
} // namespace caf

