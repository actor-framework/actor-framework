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

#define CAF_SUITE scribe_policy

#include "caf/policy/scribe_policy.hpp"

#include "caf/test/dsl.hpp"

#include "scribe_fixture.hpp"

using namespace caf;
using namespace caf::net;


CAF_TEST_FIXTURE_SCOPE(parent, parent_mock<application_mock, transport_mock>)

CAF_TEST(scibe build test) {
  auto socket_pair = make_stream_socket_pair();
  parent<application_mock, transport_mock> parent1;

  CAF_CHECK_EQUAL(true, true);
}

CAF_TEST_FIXTURE_SCOPE_END()
