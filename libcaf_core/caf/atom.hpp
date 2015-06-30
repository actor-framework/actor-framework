/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_ATOM_HPP
#define CAF_ATOM_HPP

#include <string>
#include <type_traits>

#include "caf/detail/atom_val.hpp"

namespace caf {

/// The value type of atoms.
enum class atom_value : uint64_t {
  /// @cond PRIVATE
  dirty_little_hack = 31337
  /// @endcond
};

/// Creates an atom from given string literal.
template <size_t Size>
constexpr atom_value atom(char const (&str)[Size]) {
  // last character is the NULL terminator
  static_assert(Size <= 11, "only 10 characters are allowed");
  return static_cast<atom_value>(detail::atom_val(str, 0xF));
}

/// Lifts an `atom_value` to a compile-time constant.
template <atom_value V>
struct atom_constant {
  constexpr atom_constant() {
    // nop
  }
  /// Returns the wrapped value.
  constexpr operator atom_value() const {
    return V;
  }
  /// Returns an instance *of this constant* (*not* an `atom_value`).
  static const atom_constant value;
};

template <atom_value V>
const atom_constant<V> atom_constant<V>::value = atom_constant<V>{};

/// Generic 'ADD' atom for request operations.
using add_atom = atom_constant<atom("ADD")>;

/// Generic 'GET' atom for request operations.
using get_atom = atom_constant<atom("GET")>;

/// Generic 'PUT' atom for request operations.
using put_atom = atom_constant<atom("PUT")>;

/// Generic 'DELETE' atom for request operations.
using delete_atom = atom_constant<atom("DELETE")>;

/// Generic 'OK' atom for response messages.
using ok_atom = atom_constant<atom("OK")>;

/// Generic 'ERROR' atom for response messages.
using error_atom = atom_constant<atom("ERROR")>;

/// Marker 'SYS' atom for prefixing messages to a forwarding chain
/// to address an otherwise transparent actor.
using sys_atom = atom_constant<atom("SYS")>;

/// Generic 'JOIN' atom, e.g., for signaling group subscriptions.
using join_atom = atom_constant<atom("JOIN")>;

/// Generic 'LEAVE' atom, e.g., for signaling group unsubscriptions.
using leave_atom = atom_constant<atom("LEAVE")>;

/// Generic 'FORWARD' atom, e.g., for signaling an actor that it
/// should drop the first element and forward the remainder to
/// a list of predefined receivers.
using forward_atom = atom_constant<atom("FORWARD")>;

/// Generic 'FLUSH' atom, e.g., used by `aout`.
using flush_atom = atom_constant<atom("FLUSH")>;

/// Generic 'REDIRECT' atom, e.g., used by `aout`.
using redirect_atom = atom_constant<atom("REDIRECT")>;

/// Generic 'LINK' atom for link requests over network.
using link_atom = atom_constant<atom("LINK")>;

/// Generic 'UNLINK' atom for removing networked links.
using unlink_atom = atom_constant<atom("UNLINK")>;

} // namespace caf

#endif // CAF_ATOM_HPP
