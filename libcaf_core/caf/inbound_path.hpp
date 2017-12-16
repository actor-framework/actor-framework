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

#ifndef CAF_INBOUND_PATH_HPP
#define CAF_INBOUND_PATH_HPP

#include <cstddef>
#include <cstdint>

#include "caf/actor_control_block.hpp"
#include "caf/downstream_msg.hpp"
#include "caf/stream_aborter.hpp"
#include "caf/stream_manager.hpp"
#include "caf/stream_priority.hpp"
#include "caf/stream_slot.hpp"
#include "caf/upstream_msg.hpp"

#include "caf/meta/type_name.hpp"

namespace caf {

/// State for a path to an upstream actor (source).
class inbound_path {
public:
  /// Stream aborter flag to monitor a path.
  static constexpr const auto aborter_type = stream_aborter::source_aborter;

  /// Message type for propagating graceful shutdowns.
  using regular_shutdown = upstream_msg::drop;

  /// Message type for propagating errors.
  using irregular_shutdown = upstream_msg::forced_drop;

  /// Points to the manager responsible for incoming traffic.
  stream_manager_ptr mgr;

  /// Handle to the source.
  strong_actor_ptr hdl;

  /// Stores slot IDs for sender (hdl) and receiver (self).
  stream_slots slots;

  /// Amount of credit we have signaled upstream.
  int32_t assigned_credit;

  /// Priority of incoming batches from this source.
  stream_priority prio;

  /// ID of the last acknowledged batch ID.
  int64_t last_acked_batch_id;

  /// ID of the last received batch.
  int64_t last_batch_id;

  /// Stores whether an error occurred during stream processing. Configures
  /// whether the destructor sends `close` or `forced_close` messages.
  error shutdown_reason;

  /// Keep track of measurements for the last 10 batches.
  static constexpr size_t stats_sampling_size = 16;

  /// Stores statistics for measuring complexity of incoming batches.
  struct stats_t {
    struct measurement {
      long batch_size;
      std::chrono::nanoseconds calculation_time;
    };

    struct calculation_result {
      long max_throughput;
      long items_per_batch;
    };

    stats_t();

    /// Stores `stats_sampling_size` measurements in a ring.
    std::vector<measurement> measurements;

    /// Current position in `measurements`
    size_t ring_iter;

    /// Returns the maximum number of items this actor could handle for given
    /// cycle length.
    calculation_result calculate(std::chrono::nanoseconds cycle,
                                 std::chrono::nanoseconds desired_complexity);

    /// Stores a new measurement in the ring buffer.
    void store(measurement x);
  };

  stats_t stats;

  /// Constructs a path for given handle and stream ID.
  inbound_path(stream_manager_ptr mgr_ptr, stream_slots id,
               strong_actor_ptr ptr);

  ~inbound_path();

  /// Updates `last_batch_id` and `assigned_credit`.
  //void handle_batch(long batch_size, int64_t batch_id);

  void handle(downstream_msg::batch& x);

  /// Emits a `stream_msg::ack_batch` on this path and sets `assigned_credit`
  /// to `initial_demand`.
  void emit_ack_open(local_actor* self, actor_addr rebind_from);

  /// Sends a `stream_msg::ack_batch` with credit for the next round. Credit is
  /// calculated as `max_queue_size - (assigned_credit - queued_items)`, whereas
  /// `max_queue_size` is `2 * ...`.
  /// @param self Points to the parent actor.
  /// @param queued_items Counts the accumulated size of all batches that are
  ///                     currently waiting in the mailbox.
  void emit_ack_batch(local_actor* self, long queued_items,
                      std::chrono::nanoseconds cycle,
                      std::chrono::nanoseconds desired_batch_complexity);

  /// Sends a `stream_msg::close` on this path.
  void emit_regular_shutdown(local_actor* self);

  /// Sends a `stream_msg::forced_close` on this path.
  void emit_irregular_shutdown(local_actor* self, error reason);

  /// Sends a `stream_msg::forced_close` on this path.
  static void emit_irregular_shutdown(local_actor* self, stream_slots slots,
                                      const strong_actor_ptr& hdl,
                                      error reason);
};

/// @relates inbound_path
template <class Inspector>
typename Inspector::return_type inspect(Inspector& f, inbound_path& x) {
  return f(meta::type_name("inbound_path"), x.hdl, x.slots, x.prio,
           x.last_acked_batch_id, x.last_batch_id, x.assigned_credit);
}

} // namespace caf

#endif // CAF_INBOUND_PATH_HPP
