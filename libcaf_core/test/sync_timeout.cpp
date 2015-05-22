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

#define CAF_SUITE sync_timeout
#include "caf/test/unit_test.hpp"

#include "caf/chrono.hpp"
#include "caf/thread.hpp"

#include "caf/all.hpp"

using namespace caf;

namespace {

using ping_atom = atom_constant<atom("ping")>;
using pong_atom = atom_constant<atom("pong")>;
using send_ping_atom = atom_constant<atom("send_ping")>;

behavior pong() {
  return {
    [=] (ping_atom) {
      this_thread::sleep_for(chrono::seconds(1));
      return pong_atom::value;
    }
  };
}

behavior ping1(event_based_actor* self, const actor& pong_actor) {
  self->link_to(pong_actor);
  self->send(self, send_ping_atom::value);
  return {
    [=](send_ping_atom) {
      self->sync_send(pong_actor, ping_atom::value).then(
        [=](pong_atom) {
          CAF_TEST_ERROR("received pong atom");
          self->quit(exit_reason::user_shutdown);
        },
        after(chrono::milliseconds(100)) >> [=] {
          CAF_MESSAGE("sync timeout: check");
          self->quit(exit_reason::user_shutdown);
        }
      );
    }
  };
}

behavior ping2(event_based_actor* self, const actor& pong_actor) {
  self->link_to(pong_actor);
  self->send(self, send_ping_atom::value);
  auto received_inner = std::make_shared<bool>(false);
  return {
    [=](send_ping_atom) {
      self->sync_send(pong_actor, ping_atom::value).then(
        [=](pong_atom) {
          CAF_TEST_ERROR("received pong atom");
          self->quit(exit_reason::user_shutdown);
        },
        after(chrono::milliseconds(100)) >> [=] {
          CAF_MESSAGE("inner timeout: check");
          *received_inner = true;
        }
      );
    },
    after(chrono::milliseconds(100)) >> [=] {
      CAF_CHECK_EQUAL(*received_inner, true);
      self->quit(exit_reason::user_shutdown);
    }
  };
}

struct fixture {
  ~fixture() {
    await_all_actors_done();
    shutdown();
  }
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(atom_tests, fixture)

CAF_TEST(single_timeout) {
  spawn(ping1, spawn(pong));
}

CAF_TEST(scoped_timeout) {
  spawn(ping2, spawn(pong));
}

CAF_TEST_FIXTURE_SCOPE_END()
