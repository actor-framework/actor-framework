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

#ifndef CAF_STREAM_SCATTERER_HPP
#define CAF_STREAM_SCATTERER_HPP

#include <cstddef>

#include "caf/fwd.hpp"
#include "caf/duration.hpp"
#include "caf/optional.hpp"
#include "caf/mailbox_element.hpp"

namespace caf {

/// Type-erased policy for dispatching data to sinks.
class stream_scatterer {
public:
  // -- member types -----------------------------------------------------------

  using path_type = outbound_path;

  using path_ptr = path_type*;

  // -- constructors, destructors, and assignment operators --------------------

  stream_scatterer() = default;

  virtual ~stream_scatterer();

  // -- pure virtual memeber functions -----------------------------------------

  /// Adds a path to the edge.
  /// @returns The added path on success, `nullptr` otherwise.
  virtual path_ptr add_path(const stream_id& sid, strong_actor_ptr origin,
                            strong_actor_ptr sink_ptr,
                            mailbox_element::forwarding_stack stages,
                            message_id handshake_mid, message handshake_data,
                            stream_priority prio, bool redeployable) = 0;

  /// Adds a path to a sink and initiates the handshake.
  virtual path_ptr confirm_path(const stream_id& sid, const actor_addr& from,
                                strong_actor_ptr to, long initial_demand,
                                bool redeployable) = 0;

  /// Removes a path from the scatterer.
  virtual bool remove_path(const stream_id& sid, const actor_addr& x,
                           error reason, bool silent) = 0;

  /// Returns `true` if there is no data pending and no unacknowledged batch on
  /// any path.
  virtual bool paths_clean() const = 0;

  /// Removes all paths gracefully.
  virtual void close() = 0;

  /// Removes all paths with an error message.
  virtual void abort(error reason) = 0;

  /// Returns the number of paths managed on this edge.
  virtual long num_paths() const = 0;

  /// Returns `true` if no downstream exists, `false` otherwise.
  virtual bool closed() const = 0;

  /// Returns whether this edge remains open after the last path is removed.
  virtual bool continuous() const = 0;

  /// Sets whether this edge remains open after the last path is removed.
  virtual void continuous(bool value) = 0;

  /// Sends batches to sinks.
  virtual void emit_batches() = 0;

  /// Returns the stored state for `x` if `x` is a known path and associated to
  /// `sid`, otherwise `nullptr`.
  virtual path_ptr find(const stream_id& sid, const actor_addr& x) = 0;

  /// Returns the stored state for `x` if `x` is a known path and associated to
  /// `sid`, otherwise `nullptr`.
  virtual path_ptr path_at(size_t index) = 0;

  /// Returns the currently available credit, depending on the policy in use.
  /// For example, a broadcast policy would return the minimum of all available
  /// downstream credits.
  virtual long credit() const = 0;

  /// Returns the size of the output buffer.
  virtual long buffered() const = 0;

  /// Minimum amount of messages required to emit a batch. A value of 0
  /// disables batch delays.
  virtual long min_batch_size() const = 0;

  /// Maximum amount of messages to put into a single batch. Causes the actor
  /// to split a buffer into more batches if necessary.
  virtual long max_batch_size() const = 0;

  /// Minimum amount of messages we wish to store at the actor in order to emit
  /// new batches immediately when receiving new downstream demand.
  virtual long min_buffer_size() const = 0;

  /// Forces to actor to emit a batch even if the minimum batch size was not
  /// reached.
  virtual duration max_batch_delay() const = 0;

  /// Minimum amount of messages required to emit a batch. A value of 0
  /// disables batch delays.
  virtual void min_batch_size(long x) = 0;

  /// Maximum amount of messages to put into a single batch. Causes the actor
  /// to split a buffer into more batches if necessary.
  virtual void max_batch_size(long x) = 0;

  /// Minimum amount of messages we wish to store at the actor in order to emit
  /// new batches immediately when receiving new downstream demand.
  virtual void min_buffer_size(long x) = 0;

  /// Forces to actor to emit a batch even if the minimum batch size was not
  /// reached.
  virtual void max_batch_delay(duration x) = 0;

  // -- convenience functions --------------------------------------------------

  /// Removes a path from the scatterer.
  bool remove_path(const stream_id& sid, const strong_actor_ptr& x,
                   error reason, bool silent);

  /// Convenience function for calling `find(x, actor_cast<actor_addr>(x))`.
  path_ptr find(const stream_id& sid, const strong_actor_ptr& x);
};

} // namespace caf

#endif // CAF_STREAM_SCATTERER_HPP
