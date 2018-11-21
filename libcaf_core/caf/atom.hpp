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

#include <string>
#include <functional>
#include <type_traits>

#include "caf/detail/atom_val.hpp"
#include "caf/fwd.hpp"

namespace caf {

/// The value type of atoms.
enum class atom_value : uint64_t {
  /// @cond PRIVATE
  dirty_little_hack = 31337
  /// @endcond
};

/// @relates atom_value
std::string to_string(const atom_value& what);

/// @relates atom_value
atom_value to_lowercase(atom_value x);

atom_value atom_from_string(const std::string& x);

/// Creates an atom from given string literal.
template <size_t Size>
constexpr atom_value atom(char const (&str)[Size]) {
  // last character is the NULL terminator
  static_assert(Size <= 11, "only 10 characters are allowed");
  return static_cast<atom_value>(detail::atom_val(str));
}

/// Creates an atom from given string literal and return an integer
/// representation of the atom..
template <size_t Size>
constexpr uint64_t atom_uint(char const (&str)[Size]) {
  static_assert(Size <= 11, "only 10 characters are allowed");
  return detail::atom_val(str);
}

/// Converts an atom to its integer representation.
constexpr uint64_t atom_uint(atom_value x) {
  return static_cast<uint64_t>(x);
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

  /// Returns the wrapped value as its base type.
  static constexpr uint64_t uint_value() {
    return static_cast<uint64_t>(V);
  }

  /// Returns the wrapped value.
  static constexpr atom_value get_value() {
    return V;
  }

  /// Returns an instance *of this constant* (*not* an `atom_value`).
  static const atom_constant value;
};

template <class T>
struct is_atom_constant {
  static constexpr bool value = false;
};

template <atom_value X>
struct is_atom_constant<atom_constant<X>> {
  static constexpr bool value = true;
};

template <atom_value V>
std::string to_string(const atom_constant<V>&) {
  return to_string(V);
}

template <atom_value V>
const atom_constant<V> atom_constant<V>::value = atom_constant<V>{};

/// Used for request operations.
using add_atom = atom_constant<atom("add")>;

/// Used for request operations.
using get_atom = atom_constant<atom("get")>;

/// Used for request operations.
using put_atom = atom_constant<atom("put")>;

/// Used for signalizing updates, e.g., in a key-value store.
using update_atom = atom_constant<atom("update")>;

/// Used for request operations.
using delete_atom = atom_constant<atom("delete")>;

/// Used for response messages.
using ok_atom = atom_constant<atom("ok")>;

/// Used for triggering system-level message handling.
using sys_atom = atom_constant<atom("sys")>;

/// Used for signaling group subscriptions.
using join_atom = atom_constant<atom("join")>;

/// Used for signaling group unsubscriptions.
using leave_atom = atom_constant<atom("leave")>;

/// Used for signaling forwarding paths.
using forward_atom = atom_constant<atom("forward")>;

/// Used for buffer management.
using flush_atom = atom_constant<atom("flush")>;

/// Used for I/O redirection.
using redirect_atom = atom_constant<atom("redirect")>;

/// Used for link requests over network.
using link_atom = atom_constant<atom("link")>;

/// Used for removing networked links.
using unlink_atom = atom_constant<atom("unlink")>;

/// Used for publishing actors at a given port.
using publish_atom = atom_constant<atom("publish")>;

/// Used for publishing actors at a given port.
using publish_udp_atom = atom_constant<atom("pub_udp")>;

/// Used for removing an actor/port mapping.
using unpublish_atom = atom_constant<atom("unpublish")>;

/// Used for removing an actor/port mapping.
using unpublish_udp_atom = atom_constant<atom("unpub_udp")>;

/// Used for signalizing group membership.
using subscribe_atom = atom_constant<atom("subscribe")>;

/// Used for withdrawing group membership.
using unsubscribe_atom = atom_constant<atom("unsubscrib")>;

/// Used for establishing network connections.
using connect_atom = atom_constant<atom("connect")>;

/// Used for contacting a remote UDP endpoint
using contact_atom = atom_constant<atom("contact")>;

/// Used for opening ports or files.
using open_atom = atom_constant<atom("open")>;

/// Used for closing ports or files.
using close_atom = atom_constant<atom("close")>;

/// Used for spawning remote actors.
using spawn_atom = atom_constant<atom("spawn")>;

/// Used for migrating actors to other nodes.
using migrate_atom = atom_constant<atom("migrate")>;

/// Used for triggering periodic operations.
using tick_atom = atom_constant<atom("tick")>;

/// Used for pending out of order messages.
using pending_atom = atom_constant<atom("pending")>;

/// Used as timeout type for `timeout_msg`.
using receive_atom = atom_constant<atom("receive")>;

/// Used as timeout type for `timeout_msg`.
using stream_atom = atom_constant<atom("stream")>;

} // namespace caf

namespace std {

template <>
struct hash<caf::atom_value> {
  size_t operator()(caf::atom_value x) const {
    hash<uint64_t> f;
    return f(static_cast<uint64_t>(x));
  }
};

} // namespace std

