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

#ifndef CAF_STREAM_SLOT_HPP
#define CAF_STREAM_SLOT_HPP

#include <cstdint>

#include "caf/detail/comparable.hpp"

namespace caf {

/// Identifies a single stream path in the same way a TCP port identifies a
/// connection over IP.
using stream_slot = uint16_t;

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

  inline int_fast32_t compare(stream_slots other) const noexcept {
    static_assert(sizeof(stream_slots) == sizeof(int32_t),
                  "sizeof(stream_slots) != sizeof(int32_t)");
    return reinterpret_cast<const int32_t&>(*this)
           - reinterpret_cast<int32_t&>(other);
  }
};

/// @relates stream_slots
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, stream_slots& x) {
  return f(x.sender, x.receiver);
}

} // namespace caf

#endif // CAF_STREAM_SLOT_HPP
