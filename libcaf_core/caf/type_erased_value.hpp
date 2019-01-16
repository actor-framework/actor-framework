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
#include <typeinfo>
#include <functional>

#include "caf/error.hpp"
#include "caf/fwd.hpp"
#include "caf/rtti_pair.hpp"
#include "caf/type_nr.hpp"

namespace caf {

/// Represents a single type-erased value.
class type_erased_value {
public:
  // -- constructors, destructors, and assignment operators --------------------

  virtual ~type_erased_value();

  // -- pure virtual modifiers -------------------------------------------------

  /// Returns a mutable pointer to the stored value.
  virtual void* get_mutable() = 0;

  /// Load the content for the stored value from `source`.
  virtual error load(deserializer& source) = 0;

  // -- pure virtual observers -------------------------------------------------

  /// Returns the type number and type information object for the stored value.
  virtual rtti_pair type() const = 0;

  /// Returns a pointer to the stored value.
  virtual const void* get() const = 0;

  /// Saves the content of the stored value to `sink`.
  virtual error save(serializer& sink) const = 0;

  /// Converts the stored value to a string.
  virtual std::string stringify() const = 0;

  /// Returns a copy of the stored value.
  virtual type_erased_value_ptr copy() const = 0;

  // -- observers --------------------------------------------------------------

  /// Checks whether the type of the stored value matches
  /// the type nr and type info object.
  bool matches(uint16_t nr, const std::type_info* ptr) const;

  // -- convenience functions --------------------------------------------------

  /// Returns the type number for the stored value.
  inline uint16_t type_nr() const {
    return type().first;
  }

  /// Checks whether the type of the stored value matches `rtti`.
  inline bool matches(const rtti_pair& rtti) const {
    return matches(rtti.first, rtti.second);
  }

  /// Convenience function for `reinterpret_cast<const T*>(get())`.
  template <class T>
  const T& get_as() const {
    return *reinterpret_cast<const T*>(get());
  }

  /// Convenience function for `reinterpret_cast<T*>(get_mutable())`.
  template <class T>
  T& get_mutable_as() {
    return *reinterpret_cast<T*>(get_mutable());
  }
};

/// @relates type_erased_value_impl
inline error inspect(serializer& f, type_erased_value& x) {
  return x.save(f);
}

/// @relates type_erased_value_impl
inline error inspect(deserializer& f, type_erased_value& x) {
  return x.load(f);
}

/// @relates type_erased_value_impl
inline std::string to_string(const type_erased_value& x) {
  return x.stringify();
}

} // namespace caf

