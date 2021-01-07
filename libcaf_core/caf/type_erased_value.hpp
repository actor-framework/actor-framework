// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstdint>
#include <functional>
#include <typeinfo>

#include "caf/detail/core_export.hpp"
#include "caf/error_code.hpp"
#include "caf/fwd.hpp"
#include "caf/rtti_pair.hpp"
#include "caf/type_nr.hpp"

namespace caf {

/// Represents a single type-erased value.
class CAF_CORE_EXPORT type_erased_value {
public:
  // -- constructors, destructors, and assignment operators --------------------

  virtual ~type_erased_value();

  // -- pure virtual modifiers -------------------------------------------------

  /// Returns a mutable pointer to the stored value.
  virtual void* get_mutable() = 0;

  /// Load the content for the stored value from `source`.
  virtual error load(deserializer& source) = 0;

  /// Load the content for the stored value from `source`.
  virtual error_code<sec> load(binary_deserializer& source) = 0;

  // -- pure virtual observers -------------------------------------------------

  /// Returns the type number and type information object for the stored value.
  virtual rtti_pair type() const = 0;

  /// Returns a pointer to the stored value.
  virtual const void* get() const = 0;

  /// Saves the content of the stored value to `sink`.
  virtual error save(serializer& sink) const = 0;

  /// Saves the content of the stored value to `sink`.
  virtual error_code<sec> save(binary_serializer& sink) const = 0;

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
  uint16_t type_nr() const {
    return type().first;
  }

  /// Checks whether the type of the stored value matches `rtti`.
  bool matches(const rtti_pair& rtti) const {
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

/// @relates type_erased_value
CAF_CORE_EXPORT error inspect(serializer& f, const type_erased_value& x);

/// @relates type_erased_value
CAF_CORE_EXPORT error inspect(deserializer& f, type_erased_value& x);

/// @relates type_erased_value
inline auto inspect(binary_serializer& f, const type_erased_value& x) {
  return x.save(f);
}

/// @relates type_erased_value
inline auto inspect(binary_deserializer& f, type_erased_value& x) {
  return x.load(f);
}

/// @relates type_erased_value
inline auto to_string(const type_erased_value& x) {
  return x.stringify();
}

} // namespace caf
