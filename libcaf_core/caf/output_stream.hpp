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
#include "caf/make_source_result.hpp"
#include "caf/make_stage_result.hpp"
#include "caf/stream_slot.hpp"

#include "caf/meta/type_name.hpp"

namespace caf {

/// Identifies an unbound sequence of elements annotated with the additional
/// handshake arguments emitted to the next stage.
template <class T, class... Ts>
class output_stream {
public:
  // -- member types -----------------------------------------------------------

  /// Type of a single element.
  using value_type = T;

  // -- constructors, destructors, and assignment operators --------------------

  output_stream(output_stream&&) = default;

  output_stream(const output_stream&) = default;

  template <class S>
  output_stream(make_source_result<T, S, Ts...> x)
      : in_(0),
        out_(x.out()),
        ptr_(std::move(x.ptr())) {
    // nop
  }

  template <class I, class R, class S>
  output_stream(make_stage_result<I, R, T, S, Ts...> x)
      : in_(x.in()),
        out_(x.out()),
        ptr_(std::move(x.ptr())) {
    // nop
  }

  output_stream(stream_slot in_slot, stream_slot out_slot,
                stream_manager_ptr mgr)
      : in_(in_slot),
        out_(out_slot),
        ptr_(std::move(mgr)) {
    // nop
  }

  // -- properties -------------------------------------------------------------

  /// Returns the slot of the origin stream if `ptr()` is a stage or 0 if
  /// `ptr()` is a source.
  inline stream_slot in() const {
    return in_;
  }

  /// Returns the output slot.
  inline stream_slot out() const {
    return out_;
  }

  /// Returns the handler assigned to this stream on this actor.
  inline stream_manager_ptr& ptr() noexcept {
    return ptr_;
  }

  /// Returns the handler assigned to this stream on this actor.
  inline const stream_manager_ptr& ptr() const noexcept {
    return ptr_;
  }

  // -- serialization support --------------------------------------------------

  template <class Inspector>
  friend typename Inspector::result_type inspect(Inspector& f,
                                                 output_stream& x) {
    return f(meta::type_name("output_stream"), x.in_, x.out_);
  }

private:
  // -- member variables -------------------------------------------------------

  stream_slot in_;
  stream_slot out_;
  stream_manager_ptr ptr_;
};

} // namespace caf

#endif // CAF_OUTPUT_STREAM_HPP
