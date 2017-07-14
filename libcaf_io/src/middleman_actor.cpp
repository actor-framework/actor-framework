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

#include "caf/io/middleman_actor.hpp"

#include <tuple>
#include <stdexcept>
#include <utility>

#include "caf/actor_system.hpp"
#include "caf/spawn_options.hpp"
#include "caf/actor_system_config.hpp"

#include "caf/io/middleman_actor_impl.hpp"

namespace caf {
namespace io {

middleman_actor make_middleman_actor(actor_system& sys, actor db) {
  return sys.config().middleman_detach_utility_actors
             ? sys.spawn<middleman_actor_impl, detached + hidden>(std::move(db))
             : sys.spawn<middleman_actor_impl, hidden>(std::move(db));
}

} // namespace io
} // namespace caf
