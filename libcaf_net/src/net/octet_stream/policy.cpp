#include "caf/net/octet_stream/policy.hpp"

#include "caf/net/octet_stream/errc.hpp"
#include "caf/net/stream_socket.hpp"

namespace caf::net::octet_stream {

policy::~policy() {
  // nop
}

ptrdiff_t policy::read(stream_socket x, byte_span buf) {
  return net::read(x, buf);
}

ptrdiff_t policy::write(stream_socket x, const_byte_span buf) {
  return net::write(x, buf);
}

errc policy::last_error(stream_socket, ptrdiff_t) {
  return last_socket_error_is_temporary() ? errc::temporary : errc::permanent;
}

ptrdiff_t policy::connect(stream_socket x) {
  // A connection is established if the OS reports a socket as ready for read
  // or write and if there is no error on the socket.
  return probe(x) ? 1 : -1;
}

ptrdiff_t policy::accept(stream_socket) {
  return 1;
}

size_t policy::buffered() const noexcept {
  return 0;
}

} // namespace caf::net::octet_stream
