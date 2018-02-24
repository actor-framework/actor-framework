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
  stream_manager(local_actor* selfptr,
                 stream_priority prio = stream_priority::normal);

  ~stream_manager() override;

  /// Handles `stream_msg::open` messages by creating a new slot for incoming
  /// traffic.
  /// @param slot Slot ID used by the sender, i.e., the slot ID for upstream
  ///             messages back to the sender.
  /// @param hdl Handle to the sender.
  /// @param original_stage Handle to the initial receiver of the handshake.
  /// @param priority Affects credit assignment and maximum bandwidth.
  /// @param result_cb Callback for the listener of the final stream result.
  ///                  Ignored when returning `nullptr`, because the previous
  ///                  stage is responsible for it until this manager
  ///                  acknowledges the handshake.
  /// @returns An error if the stream manager rejects the handshake.
  /// @pre `hdl != nullptr`
  /*
  virtual error open(stream_slot slot, strong_actor_ptr hdl,
                     strong_actor_ptr original_stage, stream_priority priority,
                     response_promise result_cb);
  */

  virtual void handle(inbound_path* from, downstream_msg::batch& x);

  virtual void handle(inbound_path* from, downstream_msg::close& x);

  virtual void handle(inbound_path* from, downstream_msg::forced_close& x);

  virtual void handle(stream_slots, upstream_msg::ack_open& x);

  virtual void handle(stream_slots slots, upstream_msg::ack_batch& x);

  virtual void handle(stream_slots slots, upstream_msg::drop& x);

  virtual void handle(stream_slots slots, upstream_msg::forced_drop& x);

  /// Closes all output and input paths and sends the final result to the
  /// client.
  virtual void stop();

  /// Aborts a stream after any stream message handler returned a non-default
  /// constructed error `reason` or the parent actor terminates with a
  /// non-default error.
  /// @param reason Previous error or non-default exit reason of the parent.
  virtual void abort(error reason);

  /// Pushes new data to downstream actors by sending batches. The amount of
  /// pushed data is limited by the available credit.
  virtual void push();

  /// Returns true if the handler is not able to process any further batches
  /// since it is unable to make progress sending on its own.
  virtual bool congested() const;

  /// Sends a handshake to `dest`.
  /// @pre `dest != nullptr`
  virtual void send_handshake(strong_actor_ptr dest, stream_slot slot,
                              strong_actor_ptr stream_origin,
                              mailbox_element::forwarding_stack fwd_stack,
                              message_id handshake_mid);

  /// Sends a handshake to `dest`.
  /// @pre `dest != nullptr`
  void send_handshake(strong_actor_ptr dest, stream_slot slot);

  // -- implementation hooks for sources ---------------------------------------

  /// Tries to generate new messages for the stream. This member function does
  /// nothing on stages and sinks, but can trigger a source to produce more
  /// messages.
  virtual bool generate_messages();

  // -- pure virtual member functions ------------------------------------------

  /// Returns the scatterer for outgoing data.
  virtual stream_scatterer& out() = 0;

  /// Returns whether the stream has reached the end and can be discarded
  /// safely.
  virtual bool done() const = 0;

  /// Advances time.
  virtual void cycle_timeout(size_t cycle_nr);

  // -- input path management --------------------------------------------------

  /// Informs the manager that a new input path opens.
  /// @note The lifetime of inbound paths is managed by the downstream queue.
  ///       This function is called from the constructor of `inbound_path`.
  virtual void register_input_path(inbound_path* x);

  /// Informs the manager that an input path closes.
  /// @note The lifetime of inbound paths is managed by the downstream queue.
  ///       This function is called from the destructor of `inbound_path`.
  virtual void deregister_input_path(inbound_path* x) noexcept;

  // -- mutators ---------------------------------------------------------------

  /// Adds a response promise to a sink for delivering the final result.
  /// @pre `out().terminal() == true`
  void add_promise(response_promise x);

  /// Calls `x.deliver()`
  void deliver_promises(message x);

  // -- properties -------------------------------------------------------------

  /// Returns whether this stream remains open even if no in- or outbound paths
  /// exist. The default is `false`. Does not keep a source alive past the
  /// point where its driver returns `done() == true`.
  inline bool continuous() const noexcept {
    return continuous_;
  }

  /// Sets whether this stream remains open even if no in- or outbound paths
  /// exist.
  inline void continuous(bool x) noexcept {
    continuous_ = x;
  }

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
  virtual message make_handshake(stream_slot slot) const;

  /// Called whenever new credit becomes available. The default implementation
  /// logs an error (sources are expected to override this hook).
  virtual void downstream_demand(outbound_path* ptr, long demand);

  /// Called when `out().closed()` changes to `true`. The default
  /// implementation does nothing.
  virtual void output_closed(error reason);

  /// Points to the parent actor.
  local_actor* self_;

  /// Stores non-owning pointers to all input paths.
  std::vector<inbound_path*> inbound_paths_;

  /// Keeps track of pending handshakes.
  long pending_handshakes_;

  /// Configures the importance of outgoing traffic.
  stream_priority priority_;

  /// Stores response promises for dellivering the final result.
  std::vector<response_promise> promises_;

  /// Stores promises while a handshake is active. The sink at the associated
  /// key becomes responsible for the promise after receiving `ack_open`.
  std::map<stream_slot, response_promise> in_flight_promises_;

  /// Stores whether this stream shall remain open even if no in- or outbound
  /// paths exist.
  bool continuous_;
};

/// A reference counting pointer to a `stream_manager`.
/// @relates stream_manager
using stream_manager_ptr = intrusive_ptr<stream_manager>;

} // namespace caf

#endif // CAF_STREAM_MANAGER_HPP
