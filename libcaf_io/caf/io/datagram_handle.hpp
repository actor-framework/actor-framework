// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/io/handle.hpp"

#include "caf/error.hpp"

#include <functional>

namespace caf::io {

struct invalid_datagram_handle_t {
  constexpr invalid_datagram_handle_t() {
    // nop
  }
};

constexpr invalid_datagram_handle_t invalid_datagram_handle
  = invalid_datagram_handle_t{};

/// Generic handle type for identifying datagram endpoints
class datagram_handle
  : public handle<datagram_handle, invalid_datagram_handle_t> {
public:
  friend class handle<datagram_handle, invalid_datagram_handle_t>;

  using super = handle<datagram_handle, invalid_datagram_handle_t>;

  constexpr datagram_handle() {
    // nop
  }

  constexpr datagram_handle(const invalid_datagram_handle_t&) {
    // nop
  }

  template <class Inspector>
  friend bool inspect(Inspector& f, datagram_handle& x) {
    return f.object(x).fields(f.field("id", x.id_));
  }

private:
  datagram_handle(int64_t handle_id) : super{handle_id} {
    // nop
  }
};

} // namespace caf::io

namespace std {

template <>
struct hash<caf::io::datagram_handle> {
  size_t operator()(const caf::io::datagram_handle& hdl) const {
    return std::hash<int64_t>{}(hdl.id());
  }
};

} // namespace std
