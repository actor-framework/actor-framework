// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/meta_object.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "caf/actor_system.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/config.hpp"
#include "caf/deserializer.hpp"
#include "caf/error.hpp"
#include "caf/error_code.hpp"
#include "caf/make_counted.hpp"
#include "caf/ref_counted.hpp"
#include "caf/serializer.hpp"
#include "caf/span.hpp"

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

span<const meta_object> global_meta_objects() {
  return {meta_objects, meta_objects_size};
}

const meta_object* global_meta_object(type_id_t id) {
  if (id < meta_objects_size) {
    auto& meta = meta_objects[id];
    return !meta.type_name.empty() ? &meta : nullptr;
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

span<meta_object> resize_global_meta_objects(size_t size) {
  if (size <= meta_objects_size)
    CAF_CRITICAL("resize_global_meta_objects called with a new size that does "
                 "not grow the array");
  auto new_storage = new meta_object[size];
  std::copy(meta_objects, meta_objects + meta_objects_size, new_storage);
  delete[] meta_objects;
  meta_objects = new_storage;
  meta_objects_size = size;
  return {new_storage, size};
}

void set_global_meta_objects(type_id_t first_id, span<const meta_object> xs) {
  auto new_size = first_id + xs.size();
  if (first_id < meta_objects_size) {
    if (new_size > meta_objects_size)
      CAF_CRITICAL("set_global_meta_objects called with "
                   "'first_id < meta_objects_size' and "
                   "'new_size > meta_objects_size'");
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
        auto name1 = to_string(out->type_name);
        auto name2 = to_string(x.type_name);
        CAF_CRITICAL_FMT("type ID %d already assigned to %s "
                         "(tried to override with %s)",
                         static_cast<int>(std::distance(meta_objects, out)),
                         name1.c_str(), name2.c_str());
      }
      ++out;
    }
    return;
  }
  auto dst = resize_global_meta_objects(first_id + xs.size());
  std::copy(xs.begin(), xs.end(), dst.begin() + first_id);
}

} // namespace caf::detail
