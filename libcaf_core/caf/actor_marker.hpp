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

#ifndef CAF_ACTOR_MARKER_HPP
#define CAF_ACTOR_MARKER_HPP

#include "caf/fwd.hpp"

namespace caf {

class statically_typed_actor_base {
  // used as marker only
};

class dynamically_typed_actor_base {
  // used as marker only
};

template <class T>
struct actor_marker {
  using type = statically_typed_actor_base;
};

template <>
struct actor_marker<behavior> {
  using type = dynamically_typed_actor_base;
};

} // namespace caf

#endif // CAF_ACTOR_MARKER_HPP
