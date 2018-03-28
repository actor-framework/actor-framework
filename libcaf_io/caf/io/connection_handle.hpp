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

#include <functional>

#include "caf/error.hpp"

#include "caf/io/handle.hpp"

#include "caf/meta/type_name.hpp"

namespace caf {
namespace io {

struct invalid_connection_handle_t {
  constexpr invalid_connection_handle_t() {
    // nop
  }
};

constexpr invalid_connection_handle_t invalid_connection_handle
  = invalid_connection_handle_t{};

/// Generic handle type for identifying connections.
class connection_handle : public handle<connection_handle,
                                        invalid_connection_handle_t> {
public:
  friend class handle<connection_handle, invalid_connection_handle_t>;

  using super = handle<connection_handle, invalid_connection_handle_t>;

  constexpr connection_handle() {
    // nop
  }

  constexpr connection_handle(const invalid_connection_handle_t&) {
    // nop
  }

  template <class Inspector>
  friend typename Inspector::result_type inspect(Inspector& f,
                                                 connection_handle& x) {
    return f(meta::type_name("connection_handle"), x.id_);
  }

 private:
  inline connection_handle(int64_t handle_id) : super(handle_id) {
    // nop
  }
};

} // namespace io
} // namespace caf

namespace std {

template<>
struct hash<caf::io::connection_handle> {
  size_t operator()(const caf::io::connection_handle& hdl) const {
    hash<int64_t> f;
    return f(hdl.id());
  }
};

} // namespace std

