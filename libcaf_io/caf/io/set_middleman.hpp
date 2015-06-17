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

#ifndef CAF_IO_SET_MIDDLEMAN_HPP
#define CAF_IO_SET_MIDDLEMAN_HPP

#include "caf/io/middleman.hpp"

namespace caf {
namespace io {

/// Sets a user-defined middleman using given network backend.
/// @note This function must be used before actor is spawned. Dynamically
///       changing the middleman at runtime is not supported.
/// @throws std::logic_error if a scheduler is already defined
void set_middleman(network::multiplexer* ptr);

/// Sets a user-defined middleman using given network backend.
/// @note This function must be used before actor is spawned. Dynamically
///       changing the middleman at runtime is not supported.
/// @throws std::logic_error if a scheduler is already defined
template <class Multiplexer>
void set_middleman() {
  set_middleman(new Multiplexer);
}

} // namespace io
} // namespace caf

#endif // CAF_IO_SET_MIDDLEMAN_HPP
