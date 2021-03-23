// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstddef>
#include <cstdint>

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/span.hpp"
#include "caf/string_view.hpp"

namespace caf::detail {

/// Enables destroying, construcing and serializing objects through type-erased
/// pointers.
struct meta_object {
  /// Stores a human-readable representation of the type's name.
  string_view type_name;

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
  void (*copy_construct)(void*, const void*);

  /// Applies an object to a binary serializer.
  bool (*save_binary)(caf::binary_serializer&, const void*);

  /// Applies an object to a binary deserializer.
  bool (*load_binary)(caf::binary_deserializer&, void*);

  /// Applies an object to a generic serializer.
  bool (*save)(caf::serializer&, const void*);

  /// Applies an object to a generic deserializer.
  bool (*load)(caf::deserializer&, void*);

  /// Appends a string representation of an object to a buffer.
  void (*stringify)(std::string&, const void*);
};

/// An opaque type for shared object lifetime management of the global meta
/// objects table.
using global_meta_objects_guard_type = intrusive_ptr<ref_counted>;

/// Returns a shared ownership wrapper for global state to manage meta objects.
/// Any thread that accesses the actor system should participate in the lifetime
/// management of the global state by using a meta objects guard.
CAF_CORE_EXPORT global_meta_objects_guard_type global_meta_objects_guard();

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
