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

#ifndef CAF_IO_UNPUBLISH_HPP
#define CAF_IO_UNPUBLISH_HPP

#include <cstdint>

#include "caf/actor.hpp"
#include "caf/actor_cast.hpp"
#include "caf/typed_actor.hpp"

namespace caf {
namespace io {

void unpublish_impl(const actor_addr& whom, uint16_t port, bool block_caller);

/// Unpublishes `whom` by closing `port` or all assigned ports if `port == 0`.
/// @param whom Actor that should be unpublished at `port`.
/// @param port TCP port.
template <class Handle>
void unpublish(const Handle& whom, uint16_t port = 0) {
  if (! whom) {
    return;
  }
  unpublish_impl(whom.address(), port, true);
}

} // namespace io
} // namespace caf

#endif // CAF_IO_UNPUBLISH_HPP
