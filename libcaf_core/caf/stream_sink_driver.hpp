// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <tuple>
#include <vector>

#include "caf/fwd.hpp"
#include "caf/message.hpp"
#include "caf/stream_sink.hpp"

namespace caf {

/// Identifies an unbound sequence of messages.
template <class Input>
class stream_sink_driver {
public:
  // -- member types -----------------------------------------------------------

  using input_type = Input;

  /// Implemented `stream_sink` interface.
  using sink_type = stream_sink<input_type>;

  /// Smart pointer to the interface type.
  using sink_ptr_type = intrusive_ptr<sink_type>;

  // -- constructors, destructors, and assignment operators --------------------

  virtual ~stream_sink_driver() {
    // nop
  }

  // -- virtual functions ------------------------------------------------------

  /// Called after closing the last inbound path.
  virtual void finalize(const error&) {
    // nop
  }

  // -- pure virtual functions -------------------------------------------------

  /// Processes a single batch.
  virtual void process(std::vector<input_type>& batch) = 0;

  /// Acquires credit on an inbound path. The calculated credit to fill our
  /// queue for two cycles is `desired`, but the driver is allowed to return
  /// any non-negative value.
  virtual int32_t acquire_credit(inbound_path* path, int32_t desired) {
    CAF_IGNORE_UNUSED(path);
    return desired;
  }
};

} // namespace caf
