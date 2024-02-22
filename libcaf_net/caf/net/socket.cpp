// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/socket.hpp"

#include "caf/config.hpp"
#include "caf/detail/net_syscall.hpp"
#include "caf/detail/socket_sys_includes.hpp"
#include "caf/log/net.hpp"
#include "caf/sec.hpp"

#include <system_error>

namespace caf::net {

#ifdef CAF_WINDOWS

void close(socket fd) {
  log::net::debug("close socket = {}", fd.id);
  closesocket(fd.id);
}

#  define ERRC_CASE(wsa_code, stl_code)                                        \
    case wsa_code:                                                             \
      return errc::stl_code

std::errc last_socket_error() {
  // Unfortunately, MS does not define errc consistent with the WSA error
  // codes. Hence, we cannot simply use static_cast<> but have to perform a
  // switch-case.
  using std::errc;
  int wsa_code = WSAGetLastError();
  switch (wsa_code) {
    ERRC_CASE(WSA_INVALID_HANDLE, invalid_argument);
    ERRC_CASE(WSA_NOT_ENOUGH_MEMORY, not_enough_memory);
    ERRC_CASE(WSA_INVALID_PARAMETER, invalid_argument);
    ERRC_CASE(WSAEINTR, interrupted);
    ERRC_CASE(WSAEBADF, bad_file_descriptor);
    ERRC_CASE(WSAEACCES, permission_denied);
    ERRC_CASE(WSAEFAULT, bad_address);
    ERRC_CASE(WSAEINVAL, invalid_argument);
    ERRC_CASE(WSAEMFILE, too_many_files_open);
    ERRC_CASE(WSAEWOULDBLOCK, operation_would_block);
    ERRC_CASE(WSAEINPROGRESS, operation_in_progress);
    ERRC_CASE(WSAEALREADY, connection_already_in_progress);
    ERRC_CASE(WSAENOTSOCK, not_a_socket);
    ERRC_CASE(WSAEDESTADDRREQ, destination_address_required);
    ERRC_CASE(WSAEMSGSIZE, message_size);
    ERRC_CASE(WSAEPROTOTYPE, wrong_protocol_type);
    ERRC_CASE(WSAENOPROTOOPT, no_protocol_option);
    ERRC_CASE(WSAEPROTONOSUPPORT, protocol_not_supported);
    // Windows returns this error code if the *type* argument to socket() is
    // invalid. POSIX returns EINVAL.
    ERRC_CASE(WSAESOCKTNOSUPPORT, invalid_argument);
    ERRC_CASE(WSAEOPNOTSUPP, operation_not_supported);
    // Windows returns this error code if the *protocol* argument to socket() is
    // invalid. POSIX returns EINVAL.
    ERRC_CASE(WSAEPFNOSUPPORT, invalid_argument);
    ERRC_CASE(WSAEAFNOSUPPORT, address_family_not_supported);
    ERRC_CASE(WSAEADDRINUSE, address_in_use);
    ERRC_CASE(WSAEADDRNOTAVAIL, address_not_available);
    ERRC_CASE(WSAENETDOWN, network_down);
    ERRC_CASE(WSAENETUNREACH, network_unreachable);
    ERRC_CASE(WSAENETRESET, network_reset);
    ERRC_CASE(WSAECONNABORTED, connection_aborted);
    ERRC_CASE(WSAECONNRESET, connection_reset);
    ERRC_CASE(WSAENOBUFS, no_buffer_space);
    ERRC_CASE(WSAEISCONN, already_connected);
    ERRC_CASE(WSAENOTCONN, not_connected);
    // Windows returns this error code when writing to a socket with closed
    // output channel. POSIX returns EPIPE.
    ERRC_CASE(WSAESHUTDOWN, broken_pipe);
    ERRC_CASE(WSAETIMEDOUT, timed_out);
    ERRC_CASE(WSAECONNREFUSED, connection_refused);
    ERRC_CASE(WSAELOOP, too_many_symbolic_link_levels);
    ERRC_CASE(WSAENAMETOOLONG, filename_too_long);
    ERRC_CASE(WSAEHOSTUNREACH, host_unreachable);
    ERRC_CASE(WSAENOTEMPTY, directory_not_empty);
    ERRC_CASE(WSANOTINITIALISED, network_down);
    ERRC_CASE(WSAEDISCON, already_connected);
    ERRC_CASE(WSAENOMORE, not_connected);
    ERRC_CASE(WSAECANCELLED, operation_canceled);
    ERRC_CASE(WSATRY_AGAIN, resource_unavailable_try_again);
    ERRC_CASE(WSANO_RECOVERY, state_not_recoverable);
  }
  fprintf(stderr, "[FATAL] %s:%u: unrecognized WSA error code (%d)\n", __FILE__,
          __LINE__, wsa_code);
  abort();
}

bool last_socket_error_is_temporary() {
  switch (WSAGetLastError()) {
    case WSATRY_AGAIN:
    case WSAEINPROGRESS:
    case WSAEWOULDBLOCK:
      return true;
    default:
      return false;
  }
}

std::string last_socket_error_as_string() {
  int wsa_code = WSAGetLastError();
  LPTSTR errorText = NULL;
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER
                  | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr, wsa_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPTSTR) &errorText, 0, nullptr);
  std::string result;
  if (errorText != nullptr) {
    result = errorText;
    // Release memory allocated by FormatMessage().
    LocalFree(errorText);
  }
  return result;
}

bool would_block_or_temporarily_unavailable(int errcode) {
  return errcode == WSAEWOULDBLOCK || errcode == WSATRY_AGAIN;
}

bool probe(socket x) {
  auto err = 0;
  auto len = static_cast<socklen_t>(sizeof(err));
  auto err_ptr = reinterpret_cast<char*>(&err);
  if (getsockopt(x.id, SOL_SOCKET, SO_ERROR, err_ptr, &len) == 0) {
    WSASetLastError(err);
    return err == 0;
  } else {
    return false;
  }
}

error child_process_inherit(socket x, bool) {
  // TODO: possible to implement via SetHandleInformation?
  if (x == invalid_socket)
    return make_error(sec::network_syscall_failed, "ioctlsocket",
                      "invalid socket");
  return none;
}

error nonblocking(socket x, bool new_value) {
  u_long mode = new_value ? 1 : 0;
  CAF_NET_SYSCALL("ioctlsocket", res, !=, 0, ioctlsocket(x.id, FIONBIO, &mode));
  return none;
}

#else // CAF_WINDOWS

void close(socket fd) {
  log::net::debug("close socket = {}", fd.id);
  ::close(fd.id);
}

std::errc last_socket_error() {
  // TODO: Linux and macOS both have some non-POSIX error codes that should get
  // mapped accordingly.
  return static_cast<std::errc>(errno);
}

bool last_socket_error_is_temporary() {
  switch (errno) {
    case EAGAIN:
    case EINPROGRESS:
#  if EAGAIN != EWOULDBLOCK
    case EWOULDBLOCK:
#  endif
      return true;
    default:
      return false;
  }
}

std::string last_socket_error_as_string() {
  return strerror(errno);
}

bool would_block_or_temporarily_unavailable(int errcode) {
  return errcode == EAGAIN || errcode == EWOULDBLOCK;
}

bool probe(socket x) {
  auto err = 0;
  auto len = static_cast<socklen_t>(sizeof(err));
  if (getsockopt(x.id, SOL_SOCKET, SO_ERROR, &err, &len) == 0) {
    errno = err;
    return err == 0;
  } else {
    return false;
  }
}

error child_process_inherit(socket x, bool new_value) {
  CAF_LOG_TRACE(CAF_ARG(x) << CAF_ARG(new_value));
  // read flags for x
  CAF_NET_SYSCALL("fcntl", rf, ==, -1, fcntl(x.id, F_GETFD));
  // calculate and set new flags
  auto wf = !new_value ? rf | FD_CLOEXEC : rf & ~(FD_CLOEXEC);
  CAF_NET_SYSCALL("fcntl", set_res, ==, -1, fcntl(x.id, F_SETFD, wf));
  return none;
}

error nonblocking(socket x, bool new_value) {
  CAF_LOG_TRACE(CAF_ARG(x) << CAF_ARG(new_value));
  // read flags for x
  CAF_NET_SYSCALL("fcntl", rf, ==, -1, fcntl(x.id, F_GETFL, 0));
  // calculate and set new flags
  auto wf = new_value ? (rf | O_NONBLOCK) : (rf & (~(O_NONBLOCK)));
  CAF_NET_SYSCALL("fcntl", set_res, ==, -1, fcntl(x.id, F_SETFL, wf));
  return none;
}

#endif // CAF_WINDOWS

error shutdown_read(socket x) {
  CAF_NET_SYSCALL("shutdown", res, !=, 0, shutdown(x.id, 0));
  return caf::none;
}

error shutdown_write(socket x) {
  CAF_NET_SYSCALL("shutdown", res, !=, 0, shutdown(x.id, 1));
  return caf::none;
}

} // namespace caf::net
