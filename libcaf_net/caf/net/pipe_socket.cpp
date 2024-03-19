// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/pipe_socket.hpp"

#include "caf/net/stream_socket.hpp"

#include "caf/detail/scope_guard.hpp"
#include "caf/detail/socket_sys_aliases.hpp"
#include "caf/detail/socket_sys_includes.hpp"
#include "caf/expected.hpp"
#include "caf/format_to_error.hpp"
#include "caf/log/net.hpp"
#include "caf/message.hpp"
#include "caf/sec.hpp"
#include "caf/span.hpp"

#include <cstdio>
#include <utility>

namespace caf::net {

#ifdef CAF_WINDOWS

expected<std::pair<pipe_socket, pipe_socket>> make_pipe() {
  // Windows has no support for unidirectional pipes. Hence, we emulate pipes by
  // using a regular connected socket pair.
  if (auto result = make_stream_socket_pair()) {
    return std::make_pair(socket_cast<pipe_socket>(result->first),
                          socket_cast<pipe_socket>(result->second));
  } else {
    return std::move(result.error());
  }
}

ptrdiff_t write(pipe_socket x, const_byte_span buf) {
  // On Windows, a pipe consists of two stream sockets.
  return write(socket_cast<stream_socket>(x), buf);
}

ptrdiff_t read(pipe_socket x, byte_span buf) {
  // On Windows, a pipe consists of two stream sockets.
  return read(socket_cast<stream_socket>(x), buf);
}

#else // CAF_WINDOWS

expected<std::pair<pipe_socket, pipe_socket>> make_pipe() {
  socket_id pipefds[2];
  if (pipe(pipefds) != 0)
    return format_to_error(sec::network_syscall_failed, "make_pipe failed: {}",
                           last_socket_error_as_string());
  auto guard = detail::scope_guard{[&]() noexcept {
    close(socket{pipefds[0]});
    close(socket{pipefds[1]});
  }};
  // Note: for pipe2 it is better to avoid races by setting CLOEXEC (but not on
  // POSIX).
  if (auto err = child_process_inherit(socket{pipefds[0]}, false))
    return err;
  if (auto err = child_process_inherit(socket{pipefds[1]}, false))
    return err;
  guard.disable();
  return std::make_pair(pipe_socket{pipefds[0]}, pipe_socket{pipefds[1]});
}

ptrdiff_t write(pipe_socket x, const_byte_span buf) {
  auto lg = log::net::trace("socket = {}, bytes = {}", x.id, buf.size());
  return ::write(x.id, reinterpret_cast<socket_send_ptr>(buf.data()),
                 buf.size());
}

ptrdiff_t read(pipe_socket x, byte_span buf) {
  auto lg = log::net::trace("socket = {}, bytes = {}", x.id, buf.size());
  return ::read(x.id, reinterpret_cast<socket_recv_ptr>(buf.data()),
                buf.size());
}

#endif // CAF_WINDOWS

} // namespace caf::net
