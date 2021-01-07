// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE udp_datagram_socket

#include "caf/net/udp_datagram_socket.hpp"

#include "caf/net/test/host_fixture.hpp"
#include "caf/test/dsl.hpp"

#include "caf/binary_serializer.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/detail/net_syscall.hpp"
#include "caf/detail/socket_sys_includes.hpp"
#include "caf/ip_address.hpp"
#include "caf/ip_endpoint.hpp"
#include "caf/ipv4_address.hpp"
#include "caf/net/ip.hpp"

using namespace caf;
using namespace caf::net;
using namespace caf::net::ip;

namespace {

constexpr string_view hello_test = "Hello test!";

struct fixture : host_fixture {
  fixture() : host_fixture(), buf(1024) {
    addresses = local_addresses("localhost");
    CAF_CHECK(!addresses.empty());
    ep = ip_endpoint(*addresses.begin(), 0);
    auto send_pair = unbox(make_udp_datagram_socket(ep));
    send_socket = send_pair.first;
    auto receive_pair = unbox(make_udp_datagram_socket(ep));
    receive_socket = receive_pair.first;
    ep.port(receive_pair.second);
  }

  ~fixture() {
    close(send_socket);
    close(receive_socket);
  }

  std::vector<ip_address> addresses;
  actor_system_config cfg;
  actor_system sys{cfg};
  ip_endpoint ep;
  udp_datagram_socket send_socket;
  udp_datagram_socket receive_socket;
  byte_buffer buf;
};

error read_from_socket(udp_datagram_socket sock, byte_buffer& buf) {
  uint8_t receive_attempts = 0;
  variant<std::pair<size_t, ip_endpoint>, sec> read_ret;
  do {
    read_ret = read(sock, buf);
    if (auto read_res = get_if<std::pair<size_t, ip_endpoint>>(&read_ret)) {
      buf.resize(read_res->first);
    } else if (get<sec>(read_ret) != sec::unavailable_or_would_block) {
      return make_error(get<sec>(read_ret), "read failed");
    }
    if (++receive_attempts > 100)
      return make_error(sec::runtime_error,
                        "too many unavailable_or_would_blocks");
  } while (read_ret.index() != 0);
  return none;
}

struct header {
  header(size_t payload_size) : payload_size(payload_size) {
    // nop
  }

  header() : header(0) {
    // nop
  }

  template <class Inspector>
  friend bool inspect(Inspector& f, header& x) {
    return f.object(x).fields(f.field("payload_size", x.payload_size));
  }

  size_t payload_size;
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(udp_datagram_socket_test, fixture)

CAF_TEST(socket creation) {
  ip_endpoint ep;
  CAF_CHECK_EQUAL(parse("0.0.0.0:0", ep), none);
  auto ret = make_udp_datagram_socket(ep);
  if (!ret)
    CAF_FAIL("socket creation failed: " << ret.error());
  CAF_CHECK_EQUAL(local_port(ret->first), ret->second);
}

CAF_TEST(read / write using span<byte>) {
  if (auto err = nonblocking(socket_cast<net::socket>(receive_socket), true))
    CAF_FAIL("setting socket to nonblocking failed: " << err);
  auto read_res = read(receive_socket, buf);
  if (CAF_CHECK(holds_alternative<sec>(read_res)))
    CAF_CHECK(get<sec>(read_res) == sec::unavailable_or_would_block);
  CAF_MESSAGE("sending data to " << to_string(ep));
  auto write_res = write(send_socket, as_bytes(make_span(hello_test)), ep);
  if (CAF_CHECK(holds_alternative<size_t>(write_res)))
    CAF_CHECK_EQUAL(get<size_t>(write_res), hello_test.size());
  CAF_CHECK_EQUAL(read_from_socket(receive_socket, buf), none);
  string_view received{reinterpret_cast<const char*>(buf.data()), buf.size()};
  CAF_CHECK_EQUAL(received, hello_test);
}

CAF_TEST(read / write using span<byte_buffer*>) {
  // generate header and payload in separate buffers
  header hdr{hello_test.size()};
  byte_buffer hdr_buf;
  binary_serializer sink(sys, hdr_buf);
  if (!sink.apply_object(hdr))
    CAF_FAIL("failed to serialize payload: " << sink.get_error());
  auto bytes = as_bytes(make_span(hello_test));
  byte_buffer payload_buf(bytes.begin(), bytes.end());
  auto packet_size = hdr_buf.size() + payload_buf.size();
  std::vector<byte_buffer*> bufs{&hdr_buf, &payload_buf};
  auto write_res = write(send_socket, bufs, ep);
  if (CAF_CHECK(holds_alternative<size_t>(write_res)))
    CAF_CHECK_EQUAL(get<size_t>(write_res), packet_size);
  // receive both as one single packet.
  buf.resize(packet_size);
  CAF_CHECK_EQUAL(read_from_socket(receive_socket, buf), none);
  CAF_CHECK_EQUAL(buf.size(), packet_size);
  binary_deserializer source(nullptr, buf);
  header recv_hdr;
  if (!source.apply_objects(recv_hdr))
    CAF_FAIL("failed to deserialize header: " << source.get_error());
  CAF_CHECK_EQUAL(hdr.payload_size, recv_hdr.payload_size);
  string_view received{reinterpret_cast<const char*>(buf.data())
                         + sizeof(header),
                       buf.size() - sizeof(header)};
  CAF_CHECK_EQUAL(received, hello_test);
}

CAF_TEST_FIXTURE_SCOPE_END()
