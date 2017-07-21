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

#include "caf/stream_scatterer.hpp"

#include "caf/logger.hpp"
#include "caf/actor_addr.hpp"
#include "caf/actor_cast.hpp"
#include "caf/outbound_path.hpp"

namespace caf {

stream_scatterer::~stream_scatterer() {
  // nop
}

bool stream_scatterer::remove_path(const stream_id& sid,
                                   const strong_actor_ptr& x, error reason,
                                   bool silent) {
  return remove_path(sid, actor_cast<actor_addr>(x), std::move(reason), silent);
}

stream_scatterer::path_type* stream_scatterer::find(const stream_id& sid,
                                                    const strong_actor_ptr& x) {
  return find(sid, actor_cast<actor_addr>(x));
}

} // namespace caf
