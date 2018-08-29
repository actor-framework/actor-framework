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

#include <cstdint>

#include "caf/output_stream.hpp"

#include "caf/detail/comparable.hpp"

namespace caf {

/// Identifies a single stream path in the same way a TCP port identifies a
/// connection over IP.
using stream_slot = uint16_t;

/// Identifies an invalid slot.
constexpr stream_slot invalid_stream_slot = 0;

/// Maps two `stream_slot` values into a pair for storing sender and receiver
/// slot information.
struct stream_slots : detail::comparable<stream_slots>{
  stream_slot sender;
  stream_slot receiver;

  // -- constructors, destructors, and assignment operators --------------------

  constexpr stream_slots() : sender(0), receiver(0) {
    // nop
  }

  constexpr stream_slots(stream_slot sender_slot, stream_slot receiver_slot)
      : sender(sender_slot),
        receiver(receiver_slot) {
    // nop
  }

  // -- observers --------------------------------------------------------------

  /// Returns an inverted pair, i.e., swaps sender and receiver slot.
  constexpr stream_slots invert() const {
    return {receiver, sender};
  }

  inline long compare(stream_slots other) const noexcept {
    static_assert(sizeof(long) >= sizeof(int32_t),
                  "sizeof(long) < sizeof(int32_t)");
    long x = (sender << 16) | receiver;
    long y = (other.sender << 16) | other.receiver;
    return x - y;
  }
};

/// Wraps a stream slot ID for inbound paths with the full type information of
/// the path creation.
template <class In>
class inbound_stream_slot {
public:
  // -- member types -----------------------------------------------------------

  /// Type of a single element.
  using input_type = In;

  /// The return type for `scheduled_actor::make_source`.
  using input_stream_type = stream<input_type>;

  // -- constructors, destructors, and assignment operators --------------------

  constexpr inbound_stream_slot(stream_slot value = 0): value_(value) {
    // nop
  }

  inbound_stream_slot(inbound_stream_slot&&) = default;
  inbound_stream_slot(const inbound_stream_slot&) = default;
  inbound_stream_slot& operator=(inbound_stream_slot&&) = default;
  inbound_stream_slot& operator=(const inbound_stream_slot&) = default;

  // -- conversion operators ---------------------------------------------------

  constexpr operator stream_slot() const noexcept {
    return value_;
  }

  // -- properties -------------------------------------------------------------

  constexpr stream_slot value() const noexcept {
    return value_;
  }

private:
  stream_slot value_;
};

/// Wraps a stream slot ID for outbound paths with the full type information of
/// the path creation.
template <class OutputType, class... HandshakeArgs>
class outbound_stream_slot {
public:
  // -- member types -----------------------------------------------------------

  /// Type of a single element.
  using output_type = OutputType;

  /// The return type for `scheduled_actor::make_source`.
  using output_stream_type = output_stream<output_type, HandshakeArgs...>;

  // -- constructors, destructors, and assignment operators --------------------

  constexpr outbound_stream_slot(stream_slot value = 0): value_(value) {
    // nop
  }

  outbound_stream_slot(outbound_stream_slot&&) = default;
  outbound_stream_slot(const outbound_stream_slot&) = default;
  outbound_stream_slot& operator=(outbound_stream_slot&&) = default;
  outbound_stream_slot& operator=(const outbound_stream_slot&) = default;

  // -- conversion operators ---------------------------------------------------

  constexpr operator output_stream_type() const noexcept {
    return {};
  }

  constexpr operator stream_slot() const noexcept {
    return value_;
  }

  // -- properties -------------------------------------------------------------

  constexpr stream_slot value() const noexcept {
    return value_;
  }

  // -- serialization ----------------------------------------------------------

  template <class Inspector>
  friend typename Inspector::result_type inspect(Inspector& f,
                                                 outbound_stream_slot& x) {
    return f(x.value_);
  }

private:
  stream_slot value_;
};

/// @relates stream_slots
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, stream_slots& x) {
  return f(x.sender, x.receiver);
}

} // namespace caf

