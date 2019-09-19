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

namespace {

byte operator"" _b(unsigned long long x) {
  return static_cast<byte>(x);
}

} // namespace

CAF_TEST_FIXTURE_SCOPE(network_socket_simple_tests, host_fixture)

CAF_TEST(invalid socket) {
  stream_socket x;
  CAF_CHECK_EQUAL(keepalive(x, true), sec::network_syscall_failed);
  CAF_CHECK_EQUAL(nodelay(x, true), sec::network_syscall_failed);
  CAF_CHECK_EQUAL(allow_sigpipe(x, true), sec::network_syscall_failed);
}

CAF_TEST_FIXTURE_SCOPE_END()

namespace {

struct fixture : host_fixture {
  fixture() : rd_buf(124) {
    std::tie(first, second) = unbox(make_stream_socket_pair());
    CAF_REQUIRE_EQUAL(nonblocking(first, true), caf::none);
    CAF_REQUIRE_EQUAL(nonblocking(second, true), caf::none);
    CAF_REQUIRE_NOT_EQUAL(unbox(send_buffer_size(first)), 0u);
    CAF_REQUIRE_NOT_EQUAL(unbox(send_buffer_size(second)), 0u);
  }

  ~fixture() {
    close(first);
    close(second);
  }

  stream_socket first;
  stream_socket second;
  std::vector<byte> rd_buf;
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(network_socket_tests, fixture)

CAF_TEST(read on empty sockets) {
  CAF_CHECK_EQUAL(read(first, make_span(rd_buf)),
                  sec::unavailable_or_would_block);
  CAF_CHECK_EQUAL(read(second, make_span(rd_buf)),
                  sec::unavailable_or_would_block);
}
CAF_TEST(transfer data from first to second socket) {
  std::vector<byte> wr_buf{1_b, 2_b, 4_b, 8_b, 16_b, 32_b, 64_b};
  CAF_MESSAGE("transfer data from first to second socket");
  CAF_CHECK_EQUAL(write(first, make_span(wr_buf)), wr_buf.size());
  CAF_CHECK_EQUAL(read(second, make_span(rd_buf)), wr_buf.size());
  CAF_CHECK(std::equal(wr_buf.begin(), wr_buf.end(), rd_buf.begin()));
  rd_buf.assign(rd_buf.size(), byte(0));
}

CAF_TEST(transfer data from second to first socket) {
  std::vector<byte> wr_buf{1_b, 2_b, 4_b, 8_b, 16_b, 32_b, 64_b};
  CAF_CHECK_EQUAL(write(second, make_span(wr_buf)), wr_buf.size());
  CAF_CHECK_EQUAL(read(first, make_span(rd_buf)), wr_buf.size());
  CAF_CHECK(std::equal(wr_buf.begin(), wr_buf.end(), rd_buf.begin()));
}

CAF_TEST(shut down first socket and observe shutdown on the second one) {
  close(first);
  CAF_CHECK_EQUAL(read(second, make_span(rd_buf)), sec::socket_disconnected);
  first.id = invalid_socket_id;
}

CAF_TEST(transfer data using multiple buffers) {
  std::vector<byte> wr_buf_1{1_b, 2_b, 4_b};
  std::vector<byte> wr_buf_2{8_b, 16_b, 32_b, 64_b};
  std::vector<byte> full_buf;
  full_buf.insert(full_buf.end(), wr_buf_1.begin(), wr_buf_1.end());
  full_buf.insert(full_buf.end(), wr_buf_2.begin(), wr_buf_2.end());
  CAF_CHECK_EQUAL(write(second, {make_span(wr_buf_1), make_span(wr_buf_2)}),
                  full_buf.size());
  CAF_CHECK_EQUAL(read(first, make_span(rd_buf)), full_buf.size());
  CAF_CHECK(std::equal(full_buf.begin(), full_buf.end(), rd_buf.begin()));
}

CAF_TEST_FIXTURE_SCOPE_END()
