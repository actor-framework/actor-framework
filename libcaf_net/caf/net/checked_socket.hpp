// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/socket.hpp"

#include "caf/error.hpp"
#include "caf/expected.hpp"
#include "caf/sec.hpp"

namespace caf::net {

/// Lifts `Socket` to an `expected<Socket>` and sets an error if `fd` is
/// invalid.
template <class Socket>
expected<Socket> checked_socket(Socket fd) {
  using res_t = expected<Socket>;
  if (fd.id != invalid_socket_id)
    return res_t{fd};
  else
    return res_t{make_error(sec::runtime_error, "invalid socket handle")};
}

/// A function object that calls `checked_socket`.
static constexpr auto check_socket = [](auto fd) { return checked_socket(fd); };

} // namespace caf::net
