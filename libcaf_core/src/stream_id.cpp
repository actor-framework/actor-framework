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

#include "caf/stream_id.hpp"

#include <cstddef>

namespace caf {

stream_id::stream_id() : origin(nullptr), nr(0) {
  // nop
}

stream_id::stream_id(none_t) : stream_id() {
  // nop
}

stream_id::stream_id(actor_addr origin_actor, uint64_t origin_nr)
    : origin(std::move(origin_actor)),
      nr(origin_nr) {
  // nop
}

stream_id::stream_id(actor_control_block* origin_actor, uint64_t origin_nr)
    : stream_id(origin_actor->address(), origin_nr) {
  // nop
}


stream_id::stream_id(const strong_actor_ptr& origin_actor, uint64_t origin_nr)
    : stream_id(origin_actor->address(), origin_nr) {
  // nop
}

int64_t stream_id::compare(const stream_id& other) const {
  auto r0 = static_cast<ptrdiff_t>(origin.get() - other.origin.get());
  if (r0 != 0)
    return static_cast<int64_t>(r0);
  return static_cast<int64_t>(nr) - static_cast<int64_t>(other.nr);
}

} // namespace caf
