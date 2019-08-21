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

#define CAF_SUITE stream_socket

#include "caf/net/stream_socket.hpp"

#include "caf/test/dsl.hpp"

#include "host_fixture.hpp"

#include "caf/byte.hpp"
#include "caf/span.hpp"

using namespace caf;
using namespace caf::net;

CAF_TEST_FIXTURE_SCOPE(network_socket_tests, host_fixture)

CAF_TEST(invalid socket) {
  stream_socket x;
  CAF_CHECK_EQUAL(keepalive(x, true), sec::network_syscall_failed);
  CAF_CHECK_EQUAL(nodelay(x, true), sec::network_syscall_failed);
  CAF_CHECK_EQUAL(allow_sigpipe(x, true), sec::network_syscall_failed);
}

CAF_TEST(connected socket pair) {
  std::vector<byte> wr_buf{byte(1),  byte(2),  byte(4), byte(8),
                           byte(16), byte(32), byte(64)};
  std::vector<byte> rd_buf(124);
  CAF_MESSAGE("create sockets and configure nonblocking I/O");
  auto x = unbox(make_stream_socket_pair());
  CAF_CHECK_EQUAL(nonblocking(x.first, true), caf::none);
  CAF_CHECK_EQUAL(nonblocking(x.second, true), caf::none);
  CAF_CHECK_NOT_EQUAL(unbox(send_buffer_size(x.first)), 0u);
  CAF_CHECK_NOT_EQUAL(unbox(send_buffer_size(x.second)), 0u);
  CAF_MESSAGE("verify nonblocking communication");
  CAF_CHECK_EQUAL(read(x.first, make_span(rd_buf)),
                  sec::unavailable_or_would_block);
  CAF_CHECK_EQUAL(read(x.second, make_span(rd_buf)),
                  sec::unavailable_or_would_block);
  CAF_MESSAGE("transfer data from first to second socket");
  CAF_CHECK_EQUAL(write(x.first, make_span(wr_buf)), wr_buf.size());
  CAF_CHECK_EQUAL(read(x.second, make_span(rd_buf)), wr_buf.size());
  CAF_CHECK(std::equal(wr_buf.begin(), wr_buf.end(), rd_buf.begin()));
  rd_buf.assign(rd_buf.size(), byte(0));
  CAF_MESSAGE("transfer data from second to first socket");
  CAF_CHECK_EQUAL(write(x.second, make_span(wr_buf)), wr_buf.size());
  CAF_CHECK_EQUAL(read(x.first, make_span(rd_buf)), wr_buf.size());
  CAF_CHECK(std::equal(wr_buf.begin(), wr_buf.end(), rd_buf.begin()));
  rd_buf.assign(rd_buf.size(), byte(0));
  CAF_MESSAGE("shut down first socket and observe shutdown on the second one");
  close(x.first);
  CAF_CHECK_EQUAL(read(x.second, make_span(rd_buf)), sec::socket_disconnected);
  CAF_MESSAGE("done (cleanup)");
  close(x.second);
}

CAF_TEST_FIXTURE_SCOPE_END()
