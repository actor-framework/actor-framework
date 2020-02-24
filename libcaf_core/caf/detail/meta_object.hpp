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

  /// Stores how many Bytes objects of this type require, including padding for
  /// aligning to `max_align_t`.
  size_t padded_size;

  /// Calls the destructor for given object.
  void (*destroy)(void*) noexcept;

  /// Creates a new object at given memory location by calling the default
  /// constructor.
  void (*default_construct)(void*);

  /// Creates a new object at given memory location by calling the copy
  /// constructor.
  void (*copy_construct)(const void*, void*);

  /// Applies an object to a binary serializer.
  error_code<sec> (*save_binary)(caf::binary_serializer&, const void*);

  /// Applies an object to a binary deserializer.
  error_code<sec> (*load_binary)(caf::binary_deserializer&, void*);

  /// Applies an object to a generic serializer.
  caf::error (*save)(caf::serializer&, const void*);

  /// Applies an object to a generic deserializer.
  caf::error (*load)(caf::deserializer&, void*);

  /// Appends a string representation of an object to a buffer.
  void (*stringify)(std::string&, const void*);
};

/// Convenience function for calling `meta.save(sink, obj)`.
CAF_CORE_EXPORT caf::error save(const meta_object& meta, caf::serializer& sink,
                                const void* obj);

/// Convenience function for calling `meta.save_binary(sink, obj)`.
CAF_CORE_EXPORT caf::error_code<sec>
save(const meta_object& meta, caf::binary_serializer& sink, const void* obj);

/// Convenience function for calling `meta.load(source, obj)`.
CAF_CORE_EXPORT caf::error load(const meta_object& meta,
                                caf::deserializer& source, void* obj);

/// Convenience function for calling `meta.load_binary(source, obj)`.
CAF_CORE_EXPORT caf::error_code<sec>
load(const meta_object& meta, caf::binary_deserializer& source, void* obj);

/// Returns the global storage for all meta objects. The ::type_id of an object
/// is the index for accessing the corresonding meta object.
CAF_CORE_EXPORT span<const meta_object> global_meta_objects();

/// Returns the global meta object for given type ID.
CAF_CORE_EXPORT const meta_object* global_meta_object(type_id_t id);

/// Clears the array for storing global meta objects.
/// @warning intended for unit testing only!
CAF_CORE_EXPORT void clear_global_meta_objects();

/// Resizes and returns the global storage for all meta objects. Existing
/// entries are copied to the new memory region. The new size *must* grow the
/// array.
/// @warning calling this after constructing any ::actor_system is unsafe and
///          causes undefined behavior.
CAF_CORE_EXPORT span<meta_object> resize_global_meta_objects(size_t size);

/// Sets the meta objects in range `[first_id, first_id + xs.size)` to `xs`.
/// Resizes the global meta object if needed. Aborts the program if the range
/// already contains meta objects.
/// @warning calling this after constructing any ::actor_system is unsafe and
///          causes undefined behavior.
CAF_CORE_EXPORT void set_global_meta_objects(type_id_t first_id,
                                             span<const meta_object> xs);

} // namespace caf::detail
