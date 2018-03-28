/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#include <cstddef>

#include "caf/atom.hpp"
#include "caf/deep_to_string.hpp"

#include "caf/meta/type_name.hpp"

namespace caf {

/// Stores a flow-control configuration.
struct named_actor_config {
  atom_value strategy;
  size_t low_watermark;
  size_t max_pending;
};

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, named_actor_config& x) {
  return f(meta::type_name("named_actor_config"), x.strategy, x.low_watermark,
           x.max_pending);
}

} // namespace caf

