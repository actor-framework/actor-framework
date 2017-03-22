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

#ifndef CAF_UPSTREAM_PATH_HPP
#define CAF_UPSTREAM_PATH_HPP

#include <cstddef>
#include <cstdint>

#include "caf/stream_id.hpp"
#include "caf/stream_priority.hpp"
#include "caf/actor_control_block.hpp"

#include "caf/meta/type_name.hpp"

namespace caf {

/// Denotes an upstream actor in a stream topology. Each upstream actor can
/// refer to the stream using a different stream ID.
class upstream_path {
public:
  /// Handle to the upstream actor.
  strong_actor_ptr hdl;

  /// Stream ID used on this upstream path.
  stream_id sid;

  /// Priority of this input channel.
  stream_priority prio;

  /// ID of the last received batch we have acknowledged.
  int64_t last_acked_batch_id;

  /// ID of the last received batch.
  int64_t last_batch_id;

  /// Amount of credit we have signaled upstream.
  size_t assigned_credit;

  upstream_path(strong_actor_ptr ptr, stream_id  id, stream_priority p);
};

template <class Inspector>
typename Inspector::return_type inspect(Inspector& f, upstream_path& x) {
  return f(meta::type_name("upstream_path"), x.hdl, x.sid, x.prio,
           x.last_acked_batch_id, x.last_batch_id, x.assigned_credit);
}

} // namespace caf

#endif // CAF_UPSTREAM_PATH_HPP
