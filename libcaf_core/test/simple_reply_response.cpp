/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/config.hpp"

#define CAF_SUITE simple_reply_response
#include "caf/test/unit_test.hpp"

#include "caf/all.hpp"

using namespace caf;

CAF_TEST(simple_reply_response) {
  actor_system system;
  auto s = system.spawn([](event_based_actor* self) -> behavior {
    return (
      others >> [=](const message& msg) -> message {
        CAF_CHECK(to_string(msg) == "('ok')");
        self->quit();
        return msg;
      }
    );
  });
  scoped_actor self{system};
  self->send(s, ok_atom::value);
  self->receive(
    others >> [&](const message& msg) {
      CAF_CHECK(to_string(msg) == "('ok')");
    }
  );
}
