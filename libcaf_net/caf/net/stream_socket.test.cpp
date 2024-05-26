// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/stream_socket.hpp"

#include "caf/test/test.hpp"

#include "caf/byte_buffer.hpp"
#include "caf/log/test.hpp"
#include "caf/span.hpp"

using namespace caf;
using namespace caf::net;

namespace {

std::byte operator"" _b(unsigned long long x) {
  return static_cast<std::byte>(x);
}

} // namespace

namespace {

struct fixture {
  fixture() : rd_buf(124) {
    auto maybe_sockets = make_stream_socket_pair();
    if (!maybe_sockets)
      CAF_RAISE_ERROR("cannot create connected socket pair");
    std::tie(first, second) = *maybe_sockets;
    if (auto err = nonblocking(first, true))
      CAF_RAISE_ERROR(
        detail::format("failed to set socket to nonblocking: {}", err).c_str());
    if (auto err = nonblocking(second, true))
      CAF_RAISE_ERROR(
        detail::format("failed to set socket to nonblocking: {}", err).c_str());
    if (auto buffer_size = send_buffer_size(first); buffer_size == 0u)
      CAF_RAISE_ERROR("failed to set send_buffer_size");
    if (auto buffer_size = send_buffer_size(second); buffer_size == 0u)
      CAF_RAISE_ERROR("failed to set send_buffer_size");
  }

  ~fixture() {
    close(first);
    close(second);
  }

  stream_socket first;
  stream_socket second;
  byte_buffer rd_buf;
};

WITH_FIXTURE(fixture) {

TEST("invalid socket") {
  stream_socket x;
  check_eq(keepalive(x, true), sec::network_syscall_failed);
  check_eq(nodelay(x, true), sec::network_syscall_failed);
  check_eq(allow_sigpipe(x, true), sec::network_syscall_failed);
}

TEST("read on empty sockets") {
  check_le(read(first, rd_buf), 0);
  check(last_socket_error_is_temporary());
  check_le(read(second, rd_buf), 0);
  check(last_socket_error_is_temporary());
}

TEST("transfer data from first to second socket") {
  byte_buffer wr_buf{1_b, 2_b, 4_b, 8_b, 16_b, 32_b, 64_b};
  log::test::debug("transfer data from first to second socket");
  check_eq(static_cast<size_t>(write(first, wr_buf)), wr_buf.size());
  check_eq(static_cast<size_t>(read(second, rd_buf)), wr_buf.size());
  check(std::equal(wr_buf.begin(), wr_buf.end(), rd_buf.begin()));
  rd_buf.assign(rd_buf.size(), std::byte{0});
}

TEST("transfer data from second to first socket") {
  byte_buffer wr_buf{1_b, 2_b, 4_b, 8_b, 16_b, 32_b, 64_b};
  check_eq(static_cast<size_t>(write(second, wr_buf)), wr_buf.size());
  check_eq(static_cast<size_t>(read(first, rd_buf)), wr_buf.size());
  check(std::equal(wr_buf.begin(), wr_buf.end(), rd_buf.begin()));
}

TEST("shut down first socket and observe shutdown on the second one") {
  close(first);
  check_eq(read(second, rd_buf), 0);
  first.id = invalid_socket_id;
}

TEST("transfer data using multiple buffers") {
  byte_buffer wr_buf_1{1_b, 2_b, 4_b};
  byte_buffer wr_buf_2{8_b, 16_b, 32_b, 64_b};
  byte_buffer full_buf;
  full_buf.insert(full_buf.end(), wr_buf_1.begin(), wr_buf_1.end());
  full_buf.insert(full_buf.end(), wr_buf_2.begin(), wr_buf_2.end());
  check_eq(static_cast<size_t>(
             write(second, {make_span(wr_buf_1), make_span(wr_buf_2)})),
           full_buf.size());
  check_eq(static_cast<size_t>(read(first, rd_buf)), full_buf.size());
  check(std::equal(full_buf.begin(), full_buf.end(), rd_buf.begin()));
}

} // WITH_FIXTURE(fixture)

} // namespace
