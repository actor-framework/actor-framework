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

#define CAF_SUITE pipe_socket

#include "caf/net/pipe_socket.hpp"

#include "caf/test/dsl.hpp"

#include <vector>

#include "host_fixture.hpp"

using namespace caf;
using namespace caf::net;

CAF_TEST_FIXTURE_SCOPE(pipe_socket_tests, host_fixture)

CAF_TEST(send and receive) {
  std::vector<char> send_buf{1, 2, 3, 4, 5, 6, 7, 8};
  std::vector<char> receive_buf;
  receive_buf.resize(100);
  pipe_socket rd_sock;
  pipe_socket wr_sock;
  std::tie(rd_sock, wr_sock) = unbox(make_pipe());
  CAF_CHECK_EQUAL(write(wr_sock, send_buf.data(), send_buf.size()),
                  send_buf.size());
  CAF_CHECK_EQUAL(read(rd_sock, receive_buf.data(), receive_buf.size()),
                  send_buf.size());
  CAF_CHECK(std::equal(send_buf.begin(), send_buf.end(), receive_buf.begin()));
}

CAF_TEST_FIXTURE_SCOPE_END()
