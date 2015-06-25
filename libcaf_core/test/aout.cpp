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

#define CAF_SUITE aout
#include "caf/test/unit_test.hpp"

#include "caf/all.hpp"

using namespace caf;

using std::endl;

namespace {

struct fixture {
  ~fixture() {
    await_all_actors_done();
    shutdown();
  }
};

constexpr const char* global_redirect = ":test";
constexpr const char* local_redirect = ":test2";

constexpr const char* chatty_line = "hi there! :)";
constexpr const char* chattier_line = "hello there, fellow friend! :)";

void chatty_actor(event_based_actor* self) {
  aout(self) << chatty_line << endl;
}

void chattier_actor(event_based_actor* self, const std::string& fn) {
  aout(self) << chatty_line << endl;
  actor_ostream::redirect(self, fn);
  aout(self) << chattier_line << endl;
}

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(aout_tests, fixture)

CAF_TEST(global_redirect) {
  scoped_actor self;
  self->join(group::get("local", global_redirect));
  actor_ostream::redirect_all(global_redirect);
  spawn(chatty_actor);
  self->receive(
    [](const std::string& virtual_file, const std::string& line) {
      CAF_CHECK_EQUAL(virtual_file, ":test");
      CAF_CHECK_EQUAL(line, chatty_line);
    }
  );
  self->await_all_other_actors_done();
  CAF_CHECK_EQUAL(self->mailbox().count(), 0);
}

CAF_TEST(global_and_local_redirect) {
  scoped_actor self;
  self->join(group::get("local", global_redirect));
  self->join(group::get("local", local_redirect));
  actor_ostream::redirect_all(global_redirect);
  spawn(chatty_actor);
  spawn(chattier_actor, local_redirect);
  int i = 0;
  self->receive_for(i, 2)(
    on(global_redirect, arg_match) >> [](std::string& line) {
      line.pop_back(); // drop '\n'
      CAF_CHECK_EQUAL(line, chatty_line);
    }
  );
  self->receive(
    on(local_redirect, arg_match) >> [](std::string& line) {
      line.pop_back(); // drop '\n'
      CAF_CHECK_EQUAL(line, chattier_line);
    }
  );
  self->await_all_other_actors_done();
  CAF_CHECK_EQUAL(self->mailbox().count(), 0);
}

CAF_TEST_FIXTURE_SCOPE_END()
