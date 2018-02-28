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

#ifndef CAF_OUTPUT_STREAM_HPP
#define CAF_OUTPUT_STREAM_HPP

#include "caf/fwd.hpp"
#include "caf/stream_slot.hpp"
#include "caf/stream_source.hpp"
#include "caf/stream_stage.hpp"

#include "caf/meta/type_name.hpp"

namespace caf {

/// Identifies an unbound sequence of elements annotated with the additional
/// handshake arguments emitted to the next stage.
template <class T, class... Ts>
class output_stream {
public:
  // -- member types -----------------------------------------------------------

  /// Dennotes the supertype.
  using super = stream<T>;

  /// Type of a single element.
  using value_type = T;

  /// Smart pointer to a source.
  using source_pointer = stream_source_ptr<T, Ts...>;

  // -- constructors, destructors, and assignment operators --------------------

  output_stream(output_stream&&) = default;

  output_stream(const output_stream&) = default;

  output_stream(stream_slot id, source_pointer sptr)
      : origin_(0),
        slot_(id),
        ptr_(std::move(sptr)) {
    // nop
  }

  template <class In>
  output_stream(stream_slot origin, stream_slot id,
                stream_stage_ptr<In, T, Ts...> sptr)
      : origin_(origin), slot_(id), ptr_(std::move(sptr)) {
    // nop
  }

  // -- properties -------------------------------------------------------------

  /// Returns the slot of the origin stream if `ptr()` is a stage or 0 if
  /// `ptr()` is a source.
  inline stream_slot origin_slot() const {
    return origin_;
  }

  /// Returns the output slot.
  inline stream_slot slot() const {
    return slot_;
  }

  /// Returns the handler assigned to this stream on this actor.
  inline source_pointer& ptr() noexcept {
    return ptr_;
  }

  /// Returns the handler assigned to this stream on this actor.
  inline const source_pointer& ptr() const noexcept {
    return ptr_;
  }

  // -- serialization support --------------------------------------------------

  template <class Inspector>
  friend typename Inspector::result_type inspect(Inspector& f,
                                                 output_stream& x) {
    return f(meta::type_name("output_stream"), x.origin_, x.slot_);
  }

private:
  // -- member variables -------------------------------------------------------

  stream_slot origin_;
  stream_slot slot_;
  source_pointer ptr_;
};

} // namespace caf

#endif // CAF_OUTPUT_STREAM_HPP
