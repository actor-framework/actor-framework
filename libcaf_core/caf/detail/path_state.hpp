// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <new>
#include <vector>

#include "caf/unit.hpp"

namespace caf::detail {

/// Bundles a filter and a buffer.
template <class Filter, class T>
struct path_state {
  Filter filter;
  std::vector<T> buf;
};

/// Compress path_state if `Filter` is `unit`.
template <class T>
struct path_state<unit_t, T> {
  using buffer_type = std::vector<T>;

  union {
    unit_t filter;
    buffer_type buf;
  };

  path_state() {
    new (&buf) buffer_type();
  }

  path_state(path_state&& other) {
    new (&buf) buffer_type(std::move(other.buf));
  }

  path_state(const path_state& other) {
    new (&buf) buffer_type(other.buf);
  }

  path_state& operator=(path_state&& other) {
    buf = std::move(other.buf);
    return *this;
  }

  path_state& operator=(const path_state& other) {
    buf = other.buf;
    return *this;
  }

  ~path_state() {
    buf.~buffer_type();
  }
};

} // namespace caf::detail
