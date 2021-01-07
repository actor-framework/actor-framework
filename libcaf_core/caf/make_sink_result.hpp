// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/delegated.hpp"
#include "caf/fwd.hpp"
#include "caf/stream_sink.hpp"
#include "caf/stream_slot.hpp"

namespace caf {

/// Returns a stream sink with the slot ID of its first inbound path.
template <class In>
class make_sink_result : public delegated<void> {
public:
  // -- member types -----------------------------------------------------------

  /// Type of a single element.
  using input_type = In;

  /// Fully typed stream manager as returned by `make_source`.
  using sink_type = stream_sink<In>;

  /// Pointer to a fully typed stream manager.
  using sink_ptr_type = intrusive_ptr<sink_type>;

  // -- constructors, destructors, and assignment operators --------------------

  make_sink_result() noexcept : slot_(0) {
    // nop
  }

  make_sink_result(stream_slot slot, sink_ptr_type ptr) noexcept
    : slot_(slot), ptr_(std::move(ptr)) {
    // nop
  }

  make_sink_result(make_sink_result&&) = default;
  make_sink_result(const make_sink_result&) = default;
  make_sink_result& operator=(make_sink_result&&) = default;
  make_sink_result& operator=(const make_sink_result&) = default;

  // -- properties -------------------------------------------------------------

  stream_slot inbound_slot() const noexcept {
    return slot_;
  }

  sink_ptr_type& ptr() noexcept {
    return ptr_;
  }

  const sink_ptr_type& ptr() const noexcept {
    return ptr_;
  }

private:
  // -- member variables -------------------------------------------------------

  stream_slot slot_;
  sink_ptr_type ptr_;
};

} // namespace caf
