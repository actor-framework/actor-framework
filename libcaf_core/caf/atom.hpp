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

#include <functional>
#include <iosfwd>
#include <string>
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
std::string to_string(atom_value x);

/// @relates atom_value
std::ostream& operator<<(std::ostream& out, atom_value x);

/// @relates atom_value
atom_value to_lowercase(atom_value x);

/// @relates atom_value
atom_value atom_from_string(string_view x);

/// @relates atom_value
int compare(atom_value x, atom_value y);

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

// Ugly, but pull in type_id header since it declares atom constants that used
// to live in this header.

#include "caf/type_id.hpp"
