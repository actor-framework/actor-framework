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

#define CAF_SUITE network_socket

#include "caf/net/network_socket.hpp"

#include "caf/test/dsl.hpp"

#include "host_fixture.hpp"

using namespace caf;
using namespace caf::net;

CAF_TEST_FIXTURE_SCOPE(network_socket_tests, host_fixture)

CAF_TEST(invalid socket) {
  auto x = invalid_network_socket;
  CAF_CHECK_EQUAL(keepalive(x, true), sec::network_syscall_failed);
  CAF_CHECK_EQUAL(tcp_nodelay(x, true), sec::network_syscall_failed);
  CAF_CHECK_EQUAL(allow_sigpipe(x, true), sec::network_syscall_failed);
  CAF_CHECK_EQUAL(allow_udp_connreset(x, true), sec::network_syscall_failed);
  CAF_CHECK_EQUAL(send_buffer_size(x), sec::network_syscall_failed);
  CAF_CHECK_EQUAL(local_port(x), sec::network_syscall_failed);
  CAF_CHECK_EQUAL(local_addr(x), sec::network_syscall_failed);
  CAF_CHECK_EQUAL(remote_port(x), sec::network_syscall_failed);
  CAF_CHECK_EQUAL(remote_addr(x), sec::network_syscall_failed);
}

CAF_TEST_FIXTURE_SCOPE_END()
