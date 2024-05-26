// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/io/handle.hpp"

#include "caf/error.hpp"

#include <functional>

namespace caf::io {

struct invalid_accept_handle_t {
  constexpr invalid_accept_handle_t() {
    // nop
  }
};

constexpr invalid_accept_handle_t invalid_accept_handle
  = invalid_accept_handle_t{};

/// Generic handle type for managing incoming connections.
class accept_handle : public handle<accept_handle, invalid_accept_handle_t> {
public:
  friend class handle<accept_handle, invalid_accept_handle_t>;

  using super = handle<accept_handle, invalid_accept_handle_t>;

  constexpr accept_handle() {
    // nop
  }

  constexpr accept_handle(const invalid_accept_handle_t&) {
    // nop
  }

  template <class Inspector>
  friend bool inspect(Inspector& f, accept_handle& x) {
    return f.object(x).fields(f.field("id", x.id_));
  }

private:
  accept_handle(int64_t handle_id) : super(handle_id) {
    // nop
  }
};

} // namespace caf::io

namespace std {

template <>
struct hash<caf::io::accept_handle> {
  size_t operator()(const caf::io::accept_handle& hdl) const {
    hash<int64_t> f;
    return f(hdl.id());
  }
};

} // namespace std
