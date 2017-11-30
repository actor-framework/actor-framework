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

#include "caf/stream_gatherer.hpp"

#include "caf/actor_addr.hpp"
#include "caf/actor_cast.hpp"
#include "caf/inbound_path.hpp"
#include "caf/actor_control_block.hpp"

namespace caf {

stream_gatherer::~stream_gatherer() {
  // nop
}

bool stream_gatherer::remove_path(stream_slot slot, const strong_actor_ptr& x,
                                  error reason, bool silent) {
  return remove_path(slot, actor_cast<actor_addr>(x), std::move(reason),
                     silent);
}

stream_gatherer::path_type* stream_gatherer::find(stream_slot slot,
                                                  const strong_actor_ptr& x) {
  return find(slot, actor_cast<actor_addr>(x));
}

} // namespace caf
