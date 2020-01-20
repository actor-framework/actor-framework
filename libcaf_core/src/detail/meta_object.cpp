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

#include "caf/detail/meta_object.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "caf/config.hpp"
#include "caf/span.hpp"

namespace caf::detail {

namespace {

// Stores global type information.
meta_object* meta_objects;

// Stores the size of `meta_objects`.
size_t meta_objects_size;

// Make sure to clean up all meta objects on program exit.
struct meta_objects_cleanup {
  ~meta_objects_cleanup() {
    delete[] meta_objects;
  }
} cleanup_helper;

} // namespace

span<const meta_object> global_meta_objects() {
  return {meta_objects, meta_objects_size};
}

meta_object& global_meta_object(uint16_t id) {
  CAF_ASSERT(id < meta_objects_size);
  return meta_objects[id];
}

void clear_global_meta_objects() {
  if (meta_objects != nullptr) {
    delete meta_objects;
    meta_objects = nullptr;
    meta_objects_size = 0;
  }
}

span<meta_object> resize_global_meta_objects(size_t size) {
  if (size <= meta_objects_size) {
    fprintf(stderr, "resize_global_meta_objects called with a new size that "
                    "does not grow the array\n");
    abort();
  }
  auto new_storage = new meta_object[size];
  std::copy(meta_objects, meta_objects + meta_objects_size, new_storage);
  delete[] meta_objects;
  meta_objects = new_storage;
  meta_objects_size = size;
  return {new_storage, size};
}

} // namespace caf::detail
