// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <functional>

#include "caf/error.hpp"

#include "caf/io/handle.hpp"

#include "caf/meta/type_name.hpp"

namespace caf::io {

struct invalid_connection_handle_t {
  constexpr invalid_connection_handle_t() {
    // nop
  }
};

constexpr invalid_connection_handle_t invalid_connection_handle
  = invalid_connection_handle_t{};

/// Generic handle type for identifying connections.
class connection_handle
  : public handle<connection_handle, invalid_connection_handle_t> {
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
  friend bool inspect(Inspector& f, connection_handle& x) {
    return f.object(x).fields(f.field("id", x.id_));
  }

private:
  connection_handle(int64_t handle_id) : super(handle_id) {
    // nop
  }
};

} // namespace caf::io

namespace std {

template <>
struct hash<caf::io::connection_handle> {
  size_t operator()(const caf::io::connection_handle& hdl) const {
    hash<int64_t> f;
    return f(hdl.id());
  }
};

} // namespace std
