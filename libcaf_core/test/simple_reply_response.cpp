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

#define CAF_SUITE simple_reply_response
#include "caf/test/unit_test.hpp"

#include "caf/all.hpp"

using namespace caf;

CAF_TEST(test_simple_reply_response) {
  auto s = spawn([](event_based_actor* self) -> behavior {
    return (
      others >> [=]() -> message {
        CAF_CHECK(self->current_message() == make_message(ok_atom::value));
        self->quit();
        return self->current_message();
      }
    );
  });
  {
    scoped_actor self;
    self->send(s, ok_atom::value);
    self->receive(
      others >> [&] {
        CAF_CHECK(self->current_message() == make_message(ok_atom::value));
      }
    );
  }
  await_all_actors_done();
  shutdown();
}
