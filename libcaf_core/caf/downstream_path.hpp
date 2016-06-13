/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#ifndef CAF_DOWNSTREAM_PATH_HPP
#define CAF_DOWNSTREAM_PATH_HPP

#include <deque>
#include <vector>
#include <cstdint>
#include <cstddef>

#include "caf/fwd.hpp"
#include "caf/stream_msg.hpp"

namespace caf {

/// Denotes a downstream actor in a stream topology. All downstream actors use
/// the stream ID registered with the hosting downstream object.
class downstream_path {
public:
  using topics = std::vector<atom_value>;

  /// Handle to the downstream actor.
  strong_actor_ptr ptr;

  /// Next expected batch ID.
  int64_t next_batch_id;

  /// Currently available credit for this path.
  size_t open_credit;

  /// Subscribed topics on this path (empty for all).
  topics filter;

  /// Stores whether the downstream actor is failsafe, i.e., allows the runtime
  /// to redeploy it on failure. If this field is set to `false` then
  /// `unacknowledged_batches` is unused.
  bool redeployable;

  /// Caches batches until receiving an ACK.
  std::deque<std::pair<int64_t, stream_msg::batch>> unacknowledged_batches;

  downstream_path(strong_actor_ptr p, topics ts, bool redeploy);
};

} // namespace caf

#endif // CAF_DOWNSTREAM_PATH_HPP
