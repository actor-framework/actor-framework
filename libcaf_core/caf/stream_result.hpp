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

#ifndef CAF_STREAM_RESULT_HPP
#define CAF_STREAM_RESULT_HPP

#include "caf/fwd.hpp"
#include "caf/none.hpp"
#include "caf/stream_manager.hpp"
#include "caf/stream_slot.hpp"

#include "caf/meta/type_name.hpp"

namespace caf {

/// Terminates a stream by reducing it to a single value.
template <class T, class Pointer /* = stream_manager_ptr */>
class stream_result {
public:
  // -- member types -----------------------------------------------------------

  using pointer_type = Pointer;

  // -- constructors, destructors, and assignment operators --------------------

  stream_result(stream_result&&) = default;
  stream_result(const stream_result&) = default;
  stream_result& operator=(stream_result&&) = default;
  stream_result& operator=(const stream_result&) = default;

  stream_result(none_t = none) : in_(0) {
    // nop
  }

  stream_result(stream_slot id, Pointer mgr = nullptr)
      : in_(id),
        ptr_(std::move(mgr)) {
    // nop
  }

  template <class CompatiblePointer>
  stream_result(stream_result<T, CompatiblePointer> other)
      : in_(other.in()),
        ptr_(std::move(other.ptr())) {
    // nop
  }

  template <class CompatiblePointer>
  stream_result& operator=(stream_result<T, CompatiblePointer> other) {
    in_ = other.in();
    ptr_ = std::move(other.ptr());
    return *this;
  }

  // -- properties -------------------------------------------------------------

  /// Returns the unique identifier for this stream_result.
  inline stream_slot in() const noexcept {
    return in_;
  }

  /// Returns the handler assigned to this stream on this actor.
  inline pointer_type& ptr() noexcept {
    return ptr_;
  }

  /// Returns the handler assigned to this stream on this actor.
  inline const pointer_type& ptr() const noexcept {
    return ptr_;
  }

  // -- serialization support --------------------------------------------------

  template <class Inspector>
  friend typename Inspector::result_type inspect(Inspector& f,
                                                 stream_result& x) {
    return f(meta::type_name("stream_result"), x.in_);
  }

private:
  // -- member variables -------------------------------------------------------

  stream_slot in_;
  pointer_type ptr_;
};

} // namespace caf

#endif // CAF_STREAM_RESULT_HPP
