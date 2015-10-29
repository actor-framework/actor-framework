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

#ifndef CAF_EXPERIMENTAL_WHEREIS_HPP
#define CAF_EXPERIMENTAL_WHEREIS_HPP

#include "caf/fwd.hpp"
#include "caf/atom.hpp"

namespace caf {
namespace experimental {

/// Returns the actor with the (locally) registered name.
actor whereis(atom_value registered_name);

/// Returns the actor with the registered name located at `nid` or
/// `invalid_actor` if `nid` is not connected or does not
/// respond to name lookups.
/// @warning Blocks the caller until `nid` responded to the lookup.
actor whereis(atom_value registered_name, node_id nid);

} // namespace experimental
} // namespace caf

#endif // CAF_EXPERIMENTAL_WHEREIS_HPP
