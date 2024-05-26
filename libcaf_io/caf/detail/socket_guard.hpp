// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/io/network/native_socket.hpp"

#include "caf/detail/io_export.hpp"

namespace caf::detail {

class CAF_IO_EXPORT socket_guard {
public:
  explicit socket_guard(io::network::native_socket fd);

  ~socket_guard();

  io::network::native_socket release();

  void close();

private:
  io::network::native_socket fd_;
};

} // namespace caf::detail
