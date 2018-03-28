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

#include <new>
#include <vector>

#include "caf/unit.hpp"

namespace caf {
namespace detail {

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

} // namespace detail
} // namespace caf

