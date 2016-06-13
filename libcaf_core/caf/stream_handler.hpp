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

#ifndef CAF_STREAM_HANDLER_HPP
#define CAF_STREAM_HANDLER_HPP

#include <vector>
#include <cstdint>
#include <cstddef>

#include "caf/fwd.hpp"
#include "caf/ref_counted.hpp"

namespace caf {

/// Manages a single stream with any number of down- and upstream actors.
class stream_handler : public ref_counted {
public:
  ~stream_handler();

  // -- member types -----------------------------------------------------------

  using topics = std::vector<atom_value>;

  // -- handler for downstream events ------------------------------------------

  /// Add a new downstream actor to the stream.
  /// @param hdl Handle to the new downstream actor.
  /// @param filter Subscribed topics (empty for all).
  /// @param initial_demand Credit received with `ack_open`.
  /// @param redeployable Denotes whether the runtime can redeploy
  ///                     the downstream actor on failure.
  /// @pre `hdl != nullptr`
  virtual error add_downstream(strong_actor_ptr& hdl, const topics& filter,
                               size_t initial_demand, bool redeployable);

  /// Handles ACK message from a downstream actor.
  /// @pre `hdl != nullptr`
  virtual error downstream_ack(strong_actor_ptr& hdl, int64_t batch_id);

  /// Handles new demand from a downstream actor.
  /// @pre `hdl != nullptr`
  /// @pre `new_demand > 0`
  virtual error downstream_demand(strong_actor_ptr& hdl, size_t new_demand);

  /// Push new data to downstream by sending batches. The amount of pushed data
  /// is limited by the available credit.
  virtual error push();

  // -- handler for upstream events --------------------------------------------

  /// Add a new upstream actor to the stream and return an initial credit.
  virtual expected<size_t> add_upstream(strong_actor_ptr& hdl,
                                        const stream_id& sid,
                                        stream_priority prio);

  /// Handles data from an upstream actor.
  virtual error upstream_batch(strong_actor_ptr& hdl, size_t, message& xs);

  /// Closes an upstream.
  virtual error close_upstream(strong_actor_ptr& hdl);

  /// Pull up to `n` new data items from upstream by sending a demand message.
  virtual error pull(size_t n);

  // -- handler for stream-wide events -----------------------------------------

  /// Shutdown the stream due to a fatal error.
  virtual void abort(strong_actor_ptr& cause, const error& reason) = 0;

  // -- accessors --------------------------------------------------------------

  virtual bool done() const = 0;

  /// Queries whether this handler is a sink, i.e.,
  /// does not accept downstream actors.
  virtual bool is_sink() const;

  /// Queries whether this handler is a hdl, i.e.,
  /// does not accepts upstream actors.
  virtual bool is_source() const;

  /// Returns a type-erased `stream<T>` as handshake token for downstream
  /// actors. Returns an empty message for sinks.
  virtual message make_output_token(const stream_id&) const;
};

} // namespace caf

#endif // CAF_STREAM_HANDLER_HPP
