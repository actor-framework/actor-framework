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
#include "caf/output_stream.hpp"
#include "caf/stream_slot.hpp"
#include "caf/stream_stage.hpp"

#include "caf/detail/implicit_conversions.hpp"

namespace caf {

/// Returns a stream stage with the slot IDs of its first in- and outbound
/// paths.
template <class In, class DownstreamManager, class... Ts>
class make_stage_result {
public:
  // -- member types -----------------------------------------------------------

  /// Type of a single element.
  using input_type = In;

  /// Type of a single element.
  using output_type = typename DownstreamManager::output_type;

  /// Fully typed stream manager as returned by `make_source`.
  using stage_type = stream_stage<In, DownstreamManager>;

  /// Pointer to a fully typed stream manager.
  using stage_ptr_type = intrusive_ptr<stage_type>;

  /// The return type for `scheduled_actor::make_stage`.
  using output_stream_type = output_stream<output_type, Ts...>;

  // -- constructors, destructors, and assignment operators --------------------

  make_stage_result() noexcept : inbound_slot_(0), outbound_slot_(0) {
    // nop
  }

  make_stage_result(stream_slot in, stream_slot out, stage_ptr_type ptr)
      : inbound_slot_(in),
        outbound_slot_(out),
        ptr_(std::move(ptr)) {
    // nop
  }

  make_stage_result(make_stage_result&&) = default;
  make_stage_result(const make_stage_result&) = default;
  make_stage_result& operator=(make_stage_result&&) = default;
  make_stage_result& operator=(const make_stage_result&) = default;

  // -- conversion operators ---------------------------------------------------

  inline operator output_stream_type() const noexcept {
    return {};
  }

  // -- properties -------------------------------------------------------------

  inline stream_slot inbound_slot() const noexcept {
    return inbound_slot_;
  }

  inline stream_slot outbound_slot() const noexcept {
    return outbound_slot_;
  }

  inline stage_ptr_type& ptr() noexcept {
    return ptr_;
  }

  inline const stage_ptr_type& ptr() const noexcept {
    return ptr_;
  }

private:
  stream_slot inbound_slot_;
  stream_slot outbound_slot_;
  stage_ptr_type ptr_;
};

/// Helper type for defining a `make_stage_result` from a downstream manager
/// plus additional handshake types. Hardwires `message` as result type.
template <class In, class DownstreamManager, class... Ts>
using make_stage_result_t =
  make_stage_result<In, DownstreamManager, detail::strip_and_convert_t<Ts>...>;

} // namespace caf

