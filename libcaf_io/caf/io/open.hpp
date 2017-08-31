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

#ifndef CAF_IO_OPEN_HPP
#define CAF_IO_OPEN_HPP

#include <cstdint>

#include "caf/actor_system.hpp"

#include "caf/io/middleman.hpp"

namespace caf {
namespace io {

/// Tries to open a port for other CAF instances to connect to.
/// @experimental
inline expected<uint16_t> open(actor_system& sys, uint16_t port,
                               const char* in = nullptr, bool reuse = false) {
  return sys.middleman().open(port, in, reuse);
}

} // namespace io
} // namespace caf

#endif // CAF_IO_OPEN_HPP
