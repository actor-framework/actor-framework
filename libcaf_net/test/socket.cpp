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

#define CAF_SUITE socket

#include "caf/net/socket.hpp"

#include "caf/test/dsl.hpp"

using namespace caf;
using namespace caf::net;

CAF_TEST(invalid socket) {
  auto x = invalid_socket;
  CAF_CHECK_EQUAL(x.id, invalid_socket_id);
  CAF_CHECK_EQUAL(child_process_inherit(x, true), sec::network_syscall_failed);
  CAF_CHECK_EQUAL(nonblocking(x, true), sec::network_syscall_failed);
}
