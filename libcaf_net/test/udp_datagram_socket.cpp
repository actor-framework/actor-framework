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

using namespace caf;
using namespace caf::net;
using namespace caf::net::ip;

namespace {

constexpr string_view hello_test = "Hello test!";

struct fixture : host_fixture {
  fixture() : host_fixture() {
    addresses = local_addresses("localhost");
    CAF_CHECK(!addresses.empty());
    ep = ip_endpoint(*addresses.begin(), 0);
    auto send_pair = unbox(make_udp_datagram_socket(ep));
    send_socket = send_pair.first;
    auto receive_pair = unbox(make_udp_datagram_socket(ep));
    receive_socket = receive_pair.first;
    ep.port(ntohs(receive_pair.second));
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
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(udp_datagram_socket_test, fixture)

CAF_TEST(send and receive) {
  std::vector<byte> buf(1024);
  if (auto err = nonblocking(socket_cast<net::socket>(receive_socket), true))
    CAF_FAIL("nonblocking returned an error" << err);
  CAF_CHECK_EQUAL(read(receive_socket, make_span(buf)),
                  sec::unavailable_or_would_block);
  CAF_MESSAGE("sending data to " << to_string(ep));
  CAF_CHECK_EQUAL(write(send_socket, as_bytes(make_span(hello_test)), ep),
                  hello_test.size());
  int rounds = 0;
  variant<std::pair<size_t, ip_endpoint>, sec> read_ret;
  do {
    read_ret = read(receive_socket, make_span(buf));
    if (auto read_res = get_if<std::pair<size_t, ip_endpoint>>(&read_ret)) {
      CAF_CHECK_EQUAL(read_res->first, hello_test.size());
      buf.resize(read_res->first);
    } else if (get<sec>(read_ret) != sec::unavailable_or_would_block) {
      CAF_FAIL("read returned an error: " << sys.render(get<sec>(read_ret)));
    }
    CAF_CHECK_LESS(++rounds, 100);
  } while (holds_alternative<sec>(read_ret)
           && get<sec>(read_ret) == sec::unavailable_or_would_block);
  string_view received{reinterpret_cast<const char*>(buf.data()), buf.size()};
  CAF_CHECK_EQUAL(received, hello_test);
}

CAF_TEST_FIXTURE_SCOPE_END()
