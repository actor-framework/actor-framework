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

#ifndef CAF_STREAM_MANAGER_HPP
#define CAF_STREAM_MANAGER_HPP

#include <vector>
#include <cstdint>
#include <cstddef>

#include "caf/fwd.hpp"
#include "caf/ref_counted.hpp"
#include "caf/mailbox_element.hpp"

namespace caf {

/// Manages a single stream with any number of down- and upstream actors.
/// @relates stream_msg
class stream_manager : public ref_counted {
public:
  ~stream_manager() override;

  /// Handles `stream_msg::open` messages.
  /// @returns Initial credit to the source.
  /// @param hdl Handle to the sender.
  /// @param original_stage Handle to the initial receiver of the handshake.
  /// @param priority Affects credit assignment and maximum bandwidth.
  /// @param redeployable Configures whether `hdl` could get redeployed, i.e.,
  ///                     can resume after an abort.
  /// @param result_cb Callback for the listener of the final stream result.
  ///                  Ignored when returning `nullptr`, because the previous
  ///                  stage is responsible for it until this manager
  ///                  acknowledges the handshake.
  /// @pre `hdl != nullptr`
  virtual error open(const stream_id& sid, strong_actor_ptr hdl,
                     strong_actor_ptr original_stage, stream_priority priority,
                     bool redeployable, response_promise result_cb);

  /// Handles `stream_msg::ack_open` messages, i.e., finalizes the stream
  /// handshake.
  /// @param sid ID of the outgoing stream.
  /// @param rebind_from Receiver of the original `open` message.
  /// @param rebind_to Sender of this confirmation.
  /// @param initial_demand Credit received with this `ack_open`.
  /// @param redeployable Denotes whether the runtime can redeploy
  ///                     `rebind_to` on failure.
  /// @pre `hdl != nullptr`
  virtual error ack_open(const stream_id& sid, const actor_addr& rebind_from,
                         strong_actor_ptr rebind_to, long initial_demand,
                         bool redeployable);

  /// Handles `stream_msg::batch` messages.
  /// @param hdl Handle to the sender.
  /// @param xs_size Size of the vector stored in `xs`.
  /// @param xs A type-erased vector of size `xs_size`.
  /// @param xs_id ID of this batch (must be ACKed).
  /// @pre `hdl != nullptr`
  /// @pre `xs_size > 0`
  virtual error batch(const stream_id& sid, const actor_addr& hdl, long xs_size,
                      message& xs, int64_t xs_id);

  /// Handles `stream_msg::ack_batch` messages.
  /// @param hdl Handle to the sender.
  /// @param new_demand New credit for sending data.
  /// @param cumulative_batch_id Id of last handled batch.
  /// @pre `hdl != nullptr`
  virtual error ack_batch(const stream_id& sid, const actor_addr& hdl,
                          long new_demand, int64_t cumulative_batch_id);

  /// Handles `stream_msg::close` messages.
  /// @param hdl Handle to the sender.
  /// @pre `hdl != nullptr`
  virtual error close(const stream_id& sid, const actor_addr& hdl);

  /// Handles `stream_msg::drop` messages.
  /// @param hdl Handle to the sender.
  /// @pre `hdl != nullptr`
  virtual error drop(const stream_id& sid, const actor_addr& hdl);

  /// Handles `stream_msg::drop` messages. The default implementation calls
  /// `abort(reason)` and returns `sec::unhandled_stream_error`.
  /// @param hdl Handle to the sender.
  /// @param reason Reported error from the source.
  /// @pre `hdl != nullptr`
  /// @pre `err != none`
  virtual error forced_close(const stream_id& sid, const actor_addr& hdl,
                             error reason);

  /// Handles `stream_msg::drop` messages. The default implementation calls
  /// `abort(reason)` and returns `sec::unhandled_stream_error`.
  /// @param hdl Handle to the sender.
  /// @param reason Reported error from the sink.
  /// @pre `hdl != nullptr`
  /// @pre `err != none`
  virtual error forced_drop(const stream_id& sid, const actor_addr& hdl,
                            error reason);

  /// Adds a new sink to the stream.
  virtual bool add_sink(const stream_id& sid, strong_actor_ptr origin,
                        strong_actor_ptr sink_ptr,
                        mailbox_element::forwarding_stack stages,
                        message_id handshake_mid, message handshake_data,
                        stream_priority prio, bool redeployable);

  /// Adds the source `hdl` to a stream. Convenience function for calling
  /// `in().add_path(sid, hdl).second`.
  virtual bool add_source(const stream_id& sid, strong_actor_ptr source_ptr,
                          strong_actor_ptr original_stage, stream_priority prio,
                          bool redeployable, response_promise result_cb);

  /// Pushes new data to downstream actors by sending batches. The amount of
  /// pushed data is limited by the available credit.
  virtual void push();

  /// Aborts a stream after any stream message handler returned a non-default
  /// constructed error `reason` or the parent actor terminates with a
  /// non-default error.
  /// @param reason Previous error or non-default exit reason of the parent.
  virtual void abort(error reason);

  /// Closes the stream when the parent terminates with default exit reason or
  /// the stream reached its end.
  virtual void close();

  // -- implementation hooks for all children classes --------------------------

  /// Returns the stream edge for incoming data.
  virtual stream_gatherer& in() = 0;

  /// Returns the stream edge for outgoing data.
  virtual stream_scatterer& out() = 0;

  /// Returns whether the stream has reached the end and can be discarded
  /// safely.
  virtual bool done() const = 0;

  // -- implementation hooks for sources ---------------------------------------

  /// Tries to generate new messages for the stream. This member function does
  /// nothing on stages and sinks, but can trigger a source to produce more
  /// messages.
  virtual bool generate_messages();

protected:
  // -- implementation hooks for sinks -----------------------------------------

  /// Called when the gatherer closes to produce the final stream result for
  /// all listeners. The default implementation returns an empty message.
  virtual message make_final_result();

  /// Called to handle incoming data. The default implementation logs an error
  /// (sinks are expected to override this member function).
  virtual error process_batch(message& msg);

  /// Called when `in().closed()` changes to `true`. The default
  /// implementation does nothing.
  virtual void input_closed(error reason);

  // -- implementation hooks for sources ---------------------------------------

  /// Returns a type-erased `stream<T>` as handshake token for downstream
  /// actors. Returns an empty message for sinks.
  virtual message make_output_token(const stream_id&) const;

  /// Called whenever new credit becomes available. The default implementation
  /// logs an error (sources are expected to override this hook).
  virtual void downstream_demand(outbound_path* ptr, long demand);

  /// Called when `out().closed()` changes to `true`. The default
  /// implementation does nothing.
  virtual void output_closed(error reason);

  /// Pointer to the parent actor.
  local_actor* self_;

  /// Keeps track of pending handshakes.
  
};

/// A reference counting pointer to a `stream_manager`.
/// @relates stream_manager
using stream_manager_ptr = intrusive_ptr<stream_manager>;

} // namespace caf

#endif // CAF_STREAM_MANAGER_HPP
