// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <tuple>
#include <vector>

#include "caf/fwd.hpp"
#include "caf/message.hpp"

namespace caf {

/// Encapsulates user-provided functionality for generating a stream stage.
template <class Input, class DownstreamManager>
class stream_stage_driver {
public:
  // -- member types -----------------------------------------------------------

  /// Element type of the input stream.
  using input_type = Input;

  /// Policy for distributing data to outbound paths.
  using downstream_manager_type = DownstreamManager;

  /// Element type of the output stream.
  using output_type = typename downstream_manager_type::output_type;

  /// Type of the output stream.
  using stream_type = stream<output_type>;

  /// Implemented `stream_stage` interface.
  using stage_type = stream_stage<input_type, DownstreamManager>;

  /// Smart pointer to the interface type.
  using stage_ptr_type = intrusive_ptr<stage_type>;

  // -- constructors, destructors, and assignment operators --------------------

  stream_stage_driver(DownstreamManager& out) : out_(out) {
    // nop
  }

  virtual ~stream_stage_driver() {
    // nop
  }

  // -- virtual functions ------------------------------------------------------

  /// Processes a single batch.
  virtual void
  process(downstream<output_type>& out, std::vector<input_type>& batch)
    = 0;

  /// Cleans up any state.
  virtual void finalize(const error&) {
    // nop
  }

  /// Acquires credit on an inbound path. The calculated credit to fill our
  /// queue for two cycles is `desired`, but the driver is allowed to return
  /// any non-negative value.
  virtual int32_t acquire_credit(inbound_path* path, int32_t desired) {
    CAF_IGNORE_UNUSED(path);
    return desired;
  }

protected:
  DownstreamManager& out_;
};

} // namespace caf
