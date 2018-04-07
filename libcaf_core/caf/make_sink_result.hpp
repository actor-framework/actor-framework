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

#pragma once

#include "caf/fwd.hpp"
#include "caf/stream_sink.hpp"
#include "caf/stream_slot.hpp"

namespace caf {

/// Returns a stream sink with the slot ID of its first inbound path.
template <class In>
class make_sink_result {
public:
  // -- member types -----------------------------------------------------------

  /// Type of a single element.
  using input_type = In;

  /// Fully typed stream manager as returned by `make_source`.
  using sink_type = stream_sink<In>;

  /// Pointer to a fully typed stream manager.
  using sink_ptr_type = intrusive_ptr<sink_type>;

  /// The return type for `scheduled_actor::make_sink`.
  using output_stream_type = stream<input_type>;

  // -- constructors, destructors, and assignment operators --------------------

  make_sink_result() noexcept : slot_(0) {
    // nop
  }

  make_sink_result(stream_slot slot, sink_ptr_type ptr) noexcept
      : slot_(slot),
        ptr_(std::move(ptr)) {
    // nop
  }

  make_sink_result(make_sink_result&&) = default;
  make_sink_result(const make_sink_result&) = default;
  make_sink_result& operator=(make_sink_result&&) = default;
  make_sink_result& operator=(const make_sink_result&) = default;

  // -- properties -------------------------------------------------------------

  inline stream_slot inbound_slot() const noexcept {
    return slot_;
  }

  inline sink_ptr_type& ptr() noexcept {
    return ptr_;
  }

  inline const sink_ptr_type& ptr() const noexcept {
    return ptr_;
  }

private:
  // -- member variables -------------------------------------------------------

  stream_slot slot_;
  sink_ptr_type ptr_;
};

} // namespace caf

