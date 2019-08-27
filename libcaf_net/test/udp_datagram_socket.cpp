/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE udp_datagram_socket

#include "caf/net/udp_datagram_socket.hpp"

#include "caf/test/dsl.hpp"

#include "host_fixture.hpp"

#include "caf/detail/net_syscall.hpp"
#include "caf/detail/socket_sys_includes.hpp"
#include "caf/ip_address.hpp"

using namespace caf;
using namespace caf::net;

namespace {

expected<udp_datagram_socket> make_socket() {
  CAF_NET_SYSCALL("socket", fd, ==, invalid_socket_id,
                  ::socket(AF_INET6, SOCK_DGRAM, 0));
  return udp_datagram_socket{fd};
}

struct fixture : host_fixture {
  actor_system_config cfg;
  actor_system sys{cfg};
};

constexpr string_view hello_test = "Hello test!";

} // namespace

CAF_TEST_FIXTURE_SCOPE(udp_datagram_socket_test, fixture)

CAF_TEST(send and receive) {
  std::vector<byte> buf(1024);
  ip_endpoint ep;
  if (auto err = detail::parse("[::1]:0", ep))
    CAF_FAIL("unable to parse input: " << err);
  auto sender = unbox(make_socket());
  auto receiver = unbox(make_socket());
  auto guard = detail::make_scope_guard([&] {
    close(socket_cast<net::socket>(sender));
    close(socket_cast<net::socket>(receiver));
  });
  auto port = unbox(bind(receiver, ep));
  CAF_MESSAGE("socket bound to port " << port);
  ep.port(htons(port));
  if (nonblocking(socket_cast<net::socket>(receiver), true))
    CAF_FAIL("nonblocking failed");
  auto test_read_res = read(receiver, make_span(buf));
  if (auto err = get_if<sec>(&test_read_res))
    CAF_CHECK_EQUAL(*err, sec::unavailable_or_would_block);
  else
    CAF_FAIL("read should have failed");
  auto write_ret = write(sender, as_bytes(make_span(hello_test)), ep);
  if (auto num_bytes = get_if<size_t>(&write_ret))
    CAF_CHECK_EQUAL(*num_bytes, hello_test.size());
  else
    CAF_FAIL("write returned an error: " << sys.render(get<sec>(write_ret)));
  auto read_ret = read(receiver, make_span(buf));
  if (auto read_res = get_if<std::pair<size_t, ip_endpoint>>(&read_ret)) {
    CAF_CHECK_EQUAL(read_res->first, hello_test.size());
    buf.resize(read_res->first);
  } else {
    CAF_FAIL("write returned an error: " << sys.render(get<sec>(read_ret)));
  }
  string_view buf_view{reinterpret_cast<const char*>(buf.data()), buf.size()};
  CAF_CHECK_EQUAL(buf_view, hello_test);
}

CAF_TEST_FIXTURE_SCOPE_END()
