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

#pragma once

#include <tuple>
#include <vector>

#include "caf/fwd.hpp"
#include "caf/make_message.hpp"
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

  /// Can mark the sink as congested, e.g., when writing into a buffer that
  /// fills up faster than it is drained.
  virtual bool congested() const noexcept {
    return false;
  }

  /// Acquires credit on an inbound path. The calculated credit to fill our
  /// queue fro two cycles is `desired`, but the driver is allowed to return
  /// any non-negative value.
  virtual int32_t acquire_credit(inbound_path* path, int32_t desired) {
    CAF_IGNORE_UNUSED(path);
    return desired;
  }
};

} // namespace caf

