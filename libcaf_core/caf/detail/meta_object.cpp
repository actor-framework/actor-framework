// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/meta_object.hpp"

#include "caf/actor_system.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/config.hpp"
#include "caf/deserializer.hpp"
#include "caf/detail/critical.hpp"
#include "caf/detail/panic.hpp"
#include "caf/error.hpp"
#include "caf/error_code.hpp"
#include "caf/make_counted.hpp"
#include "caf/ref_counted.hpp"
#include "caf/serializer.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <span>

namespace caf::detail {

namespace {

// Stores global type information.
detail::meta_object* meta_objects;

// Stores the size of `meta_objects`.
size_t meta_objects_size;

// Make sure to clean up all meta objects on program exit.
struct meta_objects_cleanup : ref_counted {
  ~meta_objects_cleanup() override {
    delete[] meta_objects;
  }
};

global_meta_objects_guard_type cleanup_helper
  = make_counted<meta_objects_cleanup>();

} // namespace

global_meta_objects_guard_type global_meta_objects_guard() {
  return cleanup_helper;
}

std::span<const meta_object> global_meta_objects() {
  return {meta_objects, meta_objects_size};
}

const meta_object& global_meta_object(type_id_t id) {
  if (id < meta_objects_size) {
    auto& meta = meta_objects[id];
    if (!meta.type_name.empty())
      return meta;
  }
  panic(
    "found no meta object for type ID {}!\n"
    "        This usually means that run-time type initialization is missing.\n"
    "        With CAF_MAIN, make sure to pass all custom type ID blocks.\n"
    "        With a custom main, call (before any other CAF function):\n"
    "        - caf::core::init_global_meta_objects()\n"
    "        - <module>::init_global_meta_objects() for all loaded modules\n"
    "        - caf::init_global_meta_objects<T>() for all custom ID blocks",
    id);
}

const meta_object* global_meta_object_or_null(type_id_t id) {
  if (id < meta_objects_size) {
    auto& meta = meta_objects[id];
    if (!meta.type_name.empty())
      return &meta;
  }
  return nullptr;
}

void clear_global_meta_objects() {
  if (meta_objects != nullptr) {
    delete[] meta_objects;
    meta_objects = nullptr;
    meta_objects_size = 0;
  }
}

std::span<meta_object> resize_global_meta_objects(size_t size) {
  if (size <= meta_objects_size)
    panic("resize_global_meta_objects called with a new size ({}) that does "
          "not grow the array (current size: {})",
          size, meta_objects_size);
  auto new_storage = new meta_object[size];
  std::copy(meta_objects, meta_objects + meta_objects_size, new_storage);
  delete[] meta_objects;
  meta_objects = new_storage;
  meta_objects_size = size;
  return {new_storage, size};
}

void set_global_meta_objects(type_id_t first_id,
                             std::span<const meta_object> xs) {
  auto new_size = first_id + xs.size();
  if (first_id < meta_objects_size) {
    if (new_size > meta_objects_size)
      panic("set_global_meta_objects called with "
            "'first_id ({}) < meta_objects_size ({})' and "
            "'new_size ({}) > meta_objects_size ({})'",
            first_id, meta_objects_size, new_size, meta_objects_size);
    auto out = meta_objects + first_id;
    for (const auto& x : xs) {
      if (out->type_name.empty()) {
        // We support calling set_global_meta_objects for building the global
        // table chunk-by-chunk.
        *out = x;
      } else if (out->type_name == x.type_name) {
        // nop: set_global_meta_objects implements idempotency.
      } else {
        // Get null-terminated strings.
        auto name1 = std::string{out->type_name};
        auto name2 = std::string{x.type_name};
        panic("type ID {} already assigned to {} "
              "(tried to override with {})",
              std::distance(meta_objects, out), name1, name2);
      }
      ++out;
    }
    return;
  }
  auto dst = resize_global_meta_objects(first_id + xs.size());
  std::copy(xs.begin(), xs.end(), dst.begin() + first_id);
}

} // namespace caf::detail
