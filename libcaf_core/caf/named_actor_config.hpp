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

#ifndef CAF_NAMED_ACTOR_CONFIG_HPP
#define CAF_NAMED_ACTOR_CONFIG_HPP

#include <cstddef>

#include "caf/atom.hpp"
#include "caf/deep_to_string.hpp"

namespace caf {

/// Stores a flow-control configuration.
struct named_actor_config {
  atom_value strategy;
  size_t low_watermark;
  size_t max_pending;
};

template <class Processor>
void serialize(Processor& proc, named_actor_config& x, unsigned int) {
  proc & x.strategy;
  proc & x.low_watermark;
  proc & x.max_pending;
}

inline std::string to_string(const named_actor_config& x) {
  return "named_actor_config"
         + deep_to_string_as_tuple(x.strategy, x.low_watermark, x.max_pending);
}

} // namespace caf

#endif //CAF_NAMED_ACTOR_CONFIG_HPP
