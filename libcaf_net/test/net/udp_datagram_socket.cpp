// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE net.udp_datagram_socket

#include "caf/net/udp_datagram_socket.hpp"

#include "net-test.hpp"

#include "caf/byte_buffer.hpp"
#include "caf/ip_address.hpp"
#include "caf/ip_endpoint.hpp"
#include "caf/net/ip.hpp"

using namespace caf;
using namespace caf::net;
using namespace caf::net::ip;

namespace {

constexpr std::string_view hello_test = "Hello test!";

struct fixture {
  fixture() : buf(1024) {
    auto addresses = local_addresses("localhost");
    CAF_CHECK(!addresses.empty());
    ep = ip_endpoint(addresses.front(), 0);
    send_socket = unbox(make_udp_datagram_socket(ep));
    receive_socket = unbox(make_udp_datagram_socket(ep));
    ep.port(unbox(local_port(receive_socket)));
  }

  ~fixture() {
    close(send_socket);
    close(receive_socket);
  }

  ip_endpoint ep;
  udp_datagram_socket send_socket;
  udp_datagram_socket receive_socket;
  byte_buffer buf;
};

error read_from_socket(udp_datagram_socket sock, byte_buffer& buf) {
  auto receive_attempts = 0;
  while (receive_attempts < 100) {
    auto read_res = read(sock, buf);
    if (read_res > 0) {
      buf.resize(static_cast<size_t>(read_res));
      return {};
    }
    if (!last_socket_error_is_temporary())
      return make_error(sec::socket_operation_failed,
                        last_socket_error_as_string());
    ++receive_attempts;
  }
  return make_error(sec::runtime_error, "too many read attempts");
}

} // namespace

CAF_TEST_FIXTURE_SCOPE(udp_datagram_socket_test, fixture)

CAF_TEST(socket creation) {
  ip_endpoint ep;
  CHECK_EQ(parse("0.0.0.0:0", ep), none);
  CHECK(make_udp_datagram_socket(ep));
}

CAF_TEST(read and write) {
  if (auto err = nonblocking(socket_cast<net::socket>(receive_socket), true))
    CAF_FAIL("setting socket to nonblocking failed: " << err);
  // Our first read must fail (nothing to receive yet).
  auto read_res = read(receive_socket, buf);
  CHECK(read_res < 0);
  CHECK(last_socket_error_is_temporary());
  CAF_MESSAGE("sending data to " << to_string(ep));
  auto write_res = write(send_socket, as_bytes(make_span(hello_test)), ep);
  CHECK_EQ(write_res, static_cast<ptrdiff_t>(hello_test.size()));
  CHECK_EQ(read_from_socket(receive_socket, buf), none);
  std::string_view received{reinterpret_cast<const char*>(buf.data()),
                            buf.size()};
  CHECK_EQ(received, hello_test);
}

CAF_TEST_FIXTURE_SCOPE_END()
