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
#include "caf/invalid_stream.hpp"
#include "caf/stream_slot.hpp"

#include "caf/meta/type_name.hpp"

namespace caf {

/// Identifies an unbound sequence of elements annotated with the additional
/// handshake arguments emitted to the next stage.
template <class T, class... Ts, class Pointer /*  = stream_manager_ptr */>
class output_stream<T, std::tuple<Ts...>, Pointer> {
public:
  // -- member types -----------------------------------------------------------

  /// Type of a single element.
  using value_type = T;

  /// Type of the handshake.
  using tuple_type = std::tuple<Ts...>;

  /// Type of the stream manager pointer.
  using pointer_type = Pointer;

  // -- constructors, destructors, and assignment operators --------------------

  output_stream(output_stream&&) = default;
  output_stream(const output_stream&) = default;
  output_stream& operator=(output_stream&&) = default;
  output_stream& operator=(const output_stream&) = default;

  output_stream(invalid_stream_t) : in_(0), out_(0) {
    // nop
  }

  output_stream(stream_slot in_slot, stream_slot out_slot,
                pointer_type mgr)
      : in_(in_slot),
        out_(out_slot),
        ptr_(std::move(mgr)) {
    // nop
  }

  template <class CompatiblePointer>
  output_stream(output_stream<T, tuple_type, CompatiblePointer> x)
      : in_(x.in()),
        out_(x.out()),
        ptr_(std::move(x.ptr())) {
    // nop
  }

  template <class CompatiblePointer>
  output_stream& operator=(output_stream<T, tuple_type, CompatiblePointer> x) {
    in_ = x.in();
    out_ = x.out();
    ptr_ = std::move(x.ptr());
    return *this;
  }

  // -- properties -------------------------------------------------------------

  /// Returns the slot of the origin stream if `ptr()` is a stage or 0 if
  /// `ptr()` is a source.
  inline stream_slot in() const noexcept {
    return in_;
  }

  /// Returns the output slot.
  inline stream_slot out() const noexcept {
    return out_;
  }

  /// Returns the handler assigned to this stream on this actor.
  inline pointer_type& ptr() noexcept {
    return ptr_;
  }

  /// Returns the handler assigned to this stream on this actor.
  inline const pointer_type& ptr() const noexcept {
    return ptr_;
  }

private:
  // -- member variables -------------------------------------------------------

  stream_slot in_;
  stream_slot out_;
  pointer_type ptr_;
};

/// Convenience alias for `output_stream<T, std::tuple<Ts...>>`.
template <class T, class... Ts>
using output_stream_t = output_stream<T, std::tuple<Ts...>>;

} // namespace caf

#endif // CAF_OUTPUT_STREAM_HPP
