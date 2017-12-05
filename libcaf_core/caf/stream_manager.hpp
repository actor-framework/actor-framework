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

#ifndef CAF_STREAM_MANAGER_HPP
#define CAF_STREAM_MANAGER_HPP

#include <vector>
#include <cstdint>
#include <cstddef>

#include "caf/downstream_msg.hpp"
#include "caf/fwd.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/ref_counted.hpp"
#include "caf/stream_slot.hpp"
#include "caf/upstream_msg.hpp"

namespace caf {

/// Manages a single stream with any number of down- and upstream actors.
/// @relates stream_msg
class stream_manager : public ref_counted {
public:
  stream_manager(local_actor* selfptr);

  ~stream_manager() override;

  /// Handles `stream_msg::open` messages by creating a new slot for incoming
  /// traffic.
  /// @param slot Slot ID used by the sender, i.e., the slot ID for upstream
  ///             messages back to the sender.
  /// @param hdl Handle to the sender.
  /// @param original_stage Handle to the initial receiver of the handshake.
  /// @param priority Affects credit assignment and maximum bandwidth.
  /// @param redeployable Configures whether `hdl` could get redeployed, i.e.,
  ///                     can resume after an abort.
  /// @param result_cb Callback for the listener of the final stream result.
  ///                  Ignored when returning `nullptr`, because the previous
  ///                  stage is responsible for it until this manager
  ///                  acknowledges the handshake.
  /// @returns An error if the stream manager rejects the handshake.
  /// @pre `hdl != nullptr`
  /*
  virtual error open(stream_slot slot, strong_actor_ptr hdl,
                     strong_actor_ptr original_stage, stream_priority priority,
                     bool redeployable, response_promise result_cb);
  */

  virtual error handle(inbound_path* from, downstream_msg::batch& x);

  virtual error handle(inbound_path* from, downstream_msg::close& x);

  virtual error handle(inbound_path* from, downstream_msg::forced_close& x);

  virtual error handle(outbound_path* from, upstream_msg::ack_open& x);

  virtual error handle(outbound_path* from, upstream_msg::ack_batch& x);

  virtual error handle(outbound_path* from, upstream_msg::drop& x);

  virtual error handle(outbound_path* from, upstream_msg::forced_drop& x);

  /*
  /// Adds a new sink to the stream.
  virtual bool add_sink(stream_slot slot, strong_actor_ptr origin,
                        strong_actor_ptr sink_ptr,
                        mailbox_element::forwarding_stack stages,
                        message_id handshake_mid, message handshake_data,
                        stream_priority prio, bool redeployable);

  /// Adds the source `hdl` to a stream. Convenience function for calling
  /// `in().add_path(sid, hdl).second`.
  virtual bool add_source(stream_slot slot, strong_actor_ptr source_ptr,
                          strong_actor_ptr original_stage, stream_priority prio,
                          bool redeployable, response_promise result_cb);
  */

  /// Closes the stream when the parent terminates with default exit reason or
  /// the stream reached its end.
  virtual void close();

  /// Aborts a stream after any stream message handler returned a non-default
  /// constructed error `reason` or the parent actor terminates with a
  /// non-default error.
  /// @param reason Previous error or non-default exit reason of the parent.
  virtual void abort(error reason);

  /// Pushes new data to downstream actors by sending batches. The amount of
  /// pushed data is limited by the available credit.
  virtual void push();

  // -- implementation hooks for sources ---------------------------------------

  /// Tries to generate new messages for the stream. This member function does
  /// nothing on stages and sinks, but can trigger a source to produce more
  /// messages.
  virtual bool generate_messages();

  // -- pure virtual member functions ------------------------------------------

  /// Returns the stream edge for outgoing data.
  virtual stream_scatterer& out() = 0;

  /// Returns whether the stream has reached the end and can be discarded
  /// safely.
  virtual bool done() const = 0;

  // -- input path management --------------------------------------------------

  /// Informs the manager that a new input path opens.
  virtual void register_input_path(inbound_path* x);

  /// Informs the manager that an input path closes.
  virtual void deregister_input_path(inbound_path* x) noexcept;

  // -- inline functions -------------------------------------------------------

  inline local_actor* self() {
    return self_;
  }

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
