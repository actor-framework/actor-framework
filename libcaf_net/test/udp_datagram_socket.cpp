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
#include "caf/ip_endpoint.hpp"
#include "caf/ipv4_address.hpp"
#include "caf/net/ip.hpp"
#include "caf/net/socket_guard.hpp"

using namespace caf;
using namespace caf::net;

namespace {

constexpr string_view hello_test = "Hello test!";

struct fixture : host_fixture {
  fixture()
    : host_fixture(),
      v4_local{make_ipv4_address(127, 0, 0, 1)},
      v6_local{{0}, {0x1}},
      // TODO: use local_addresses() when merged
      addrs{net::ip::resolve("localhost")} {
  }

  bool contains(ip_address x) {
    return std::count(addrs.begin(), addrs.end(), x) > 0;
  }

  void test_send_receive(ip_address addr) {
    std::vector<byte> buf(1024);
    ip_endpoint ep(addr, 0);
    auto send_pair = unbox(make_udp_datagram_socket(ep));
    auto send_socket = send_pair.first;
    auto receive_pair = unbox(make_udp_datagram_socket(ep));
    auto receive_socket = receive_pair.first;
    ep.port(ntohs(receive_pair.second));
    CAF_MESSAGE("sending data to: " << to_string(ep));
    auto send_guard = make_socket_guard(send_socket);
    auto receive_guard = make_socket_guard(receive_socket);
    if (auto err = nonblocking(socket_cast<net::socket>(receive_socket), true))
      CAF_FAIL("nonblocking returned an error" << err);
    auto test_read_res = read(receive_socket, make_span(buf));
    if (auto err = get_if<sec>(&test_read_res))
      CAF_CHECK_EQUAL(*err, sec::unavailable_or_would_block);
    else
      CAF_FAIL("read should have failed");

    auto write_ret = write(send_socket, as_bytes(make_span(hello_test)), ep);
    if (auto num_bytes = get_if<size_t>(&write_ret))
      CAF_CHECK_EQUAL(*num_bytes, hello_test.size());
    else
      CAF_FAIL("write returned an error: " << sys.render(get<sec>(write_ret)));

    auto read_ret = read(receive_socket, make_span(buf));
    if (auto read_res = get_if<std::pair<size_t, ip_endpoint>>(&read_ret)) {
      CAF_CHECK_EQUAL(read_res->first, hello_test.size());
      buf.resize(read_res->first);
    } else {
      CAF_FAIL("write returned an error: " << sys.render(get<sec>(read_ret)));
    }
    string_view buf_view{reinterpret_cast<const char*>(buf.data()), buf.size()};
    CAF_CHECK_EQUAL(buf_view, hello_test);
  }

  actor_system_config cfg;
  actor_system sys{cfg};
  ip_address v4_local;
  ip_address v6_local;
  std::vector<ip_address> addrs;
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(udp_datagram_socket_test, fixture)

CAF_TEST_DISABLED(send and receive) {
  // TODO: check which versions exist and test existing versions accordingly
  // -> local_addresses()
  if (contains(v4_local)) {
    test_send_receive(v4_local);
  } else if (contains(v6_local)) {
    test_send_receive(v6_local);
  } else {
    CAF_FAIL("could not resolve 'localhost'");
  }
}

CAF_TEST_FIXTURE_SCOPE_END()
