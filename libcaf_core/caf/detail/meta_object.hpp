/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
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

#include <cstddef>
#include <cstdint>

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

namespace caf::detail {

/// Enables destroying, construcing and serializing objects through type-erased
/// pointers.
struct meta_object {
  /// Stores a human-readable representation of the type's name.
  const char* type_name = nullptr;

  /// Calls the destructor for given object.
  void (*destroy)(void*) noexcept;

  /// Creates a new object at given memory location by calling the default
  /// constructor.
  void (*default_construct)(void*);

  /// Applies an object to a binary serializer.
  error_code<sec> (*save_binary)(caf::binary_serializer&, const void*);

  /// Applies an object to a binary deserializer.
  error_code<sec> (*load_binary)(caf::binary_deserializer&, void*);

  /// Applies an object to a generic serializer.
  caf::error (*save)(caf::serializer&, const void*);

  /// Applies an object to a generic deserializer.
  caf::error (*load)(caf::deserializer&, void*);
};

/// Returns the global storage for all meta objects. The ::type_id of an object
/// is the index for accessing the corresonding meta object.
CAF_CORE_EXPORT span<const meta_object> global_meta_objects();

/// Returns the global meta object for given type ID.
CAF_CORE_EXPORT meta_object& global_meta_object(uint16_t id);

/// Clears the array for storing global meta objects.
/// @warning intended for unit testing only!
CAF_CORE_EXPORT void clear_global_meta_objects();

/// Resizes and returns the global storage for all meta objects. Existing
/// entries are copied to the new memory region. The new size *must* grow the
/// array.
/// @warning calling this after constructing any ::actor_system is unsafe and
///          causes undefined behavior.
CAF_CORE_EXPORT span<meta_object> resize_global_meta_objects(size_t size);

/// Sets the meta objects in range `(first_id, first_id + xs.size]` to `xs`.
/// Resizes the global meta object if needed. Aborts the program if the range
/// already contains meta objects.
/// @warning calling this after constructing any ::actor_system is unsafe and
///          causes undefined behavior.
CAF_CORE_EXPORT void set_global_meta_objects(uint16_t first_id,
                                             span<const meta_object> xs);

} // namespace caf::detail
