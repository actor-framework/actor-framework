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

#ifndef CAF_STREAM_HPP
#define CAF_STREAM_HPP

#include "caf/fwd.hpp"
#include "caf/stream_slot.hpp"
#include "caf/stream_manager.hpp"

#include "caf/meta/type_name.hpp"

namespace caf {

/// Marker type for constructing invalid `stream` objects.
struct invalid_stream_t {};

/// Identifies an unbound sequence of messages.
template <class T>
class stream {
public:
  // -- member types -----------------------------------------------------------

  using value_type = T;

  // -- constructors and destructors -------------------------------------------

  stream(stream&&) = default;
  stream(const stream&) = default;
  stream& operator=(stream&&) = default;
  stream& operator=(const stream&) = default;

  stream(none_t = none) : slot_(0) {
    // nop
  }

  stream(invalid_stream_t) : stream(none) {
    // nop
  }

  /// Convenience constructor for returning the result of `self->new_stream`
  /// and similar functions.
  stream(stream_slot id, stream_manager_ptr sptr = nullptr)
      : slot_(id),
        ptr_(std::move(sptr)) {
    // nop
  }

  /// Convenience constructor for returning the result of `self->new_stream`
  /// and similar functions.
  stream(stream other,  stream_manager_ptr sptr)
      : slot_(std::move(other.slot_)),
        ptr_(std::move(sptr)) {
    // nop
  }

  // -- accessors --------------------------------------------------------------

  /// Returns the actor-specific stream slot ID.
  inline stream_slot slot() const {
    return slot_;
  }

  /// Returns the handler assigned to this stream on this actor.
  inline const stream_manager_ptr& ptr() const {
    return ptr_;
  }

  // -- serialization support --------------------------------------------------

  template <class Inspector>
  friend typename Inspector::result_type inspect(Inspector& f, stream& x) {
    return f(meta::type_name("stream"), x.slot_);
  }

private:
  // -- member variables -------------------------------------------------------

  stream_slot slot_;
  stream_manager_ptr ptr_;
};

/// @relates stream
constexpr invalid_stream_t invalid_stream = invalid_stream_t{};

/// Identifies an unbound sequence of messages annotated with the additional
/// handshake arguments emitted to the next stage.
template <class T, class... Ts>
class annotated_stream final : public stream<T> {
public:
  /// Dennotes the supertype.
  using super = stream<T>;

  // Import constructors.
  using super::super;
};

} // namespace caf

#endif // CAF_STREAM_HPP
