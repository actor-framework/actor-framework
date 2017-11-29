/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
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

constexpr const char* global_redirect = ":test";
constexpr const char* local_redirect = ":test2";

constexpr const char* chatty_line = "hi there!:)";
constexpr const char* chattier_line = "hello there, fellow friend!:)";

void chatty_actor(event_based_actor* self) {
  aout(self) << chatty_line << endl;
}

void chattier_actor(event_based_actor* self, const std::string& fn) {
  aout(self) << chatty_line << endl;
  actor_ostream::redirect(self, fn);
  aout(self) << chattier_line << endl;
}

struct fixture {
  fixture() : system(cfg) {
    // nop
  }

  actor_system_config cfg;
  actor_system system;
  scoped_actor self{system, true};
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(adapter_tests, fixture)

CAF_TEST(redirect_aout_globally) {
  self->join(system.groups().get_local(global_redirect));
  actor_ostream::redirect_all(system, global_redirect);
  system.spawn(chatty_actor);
  self->receive(
    [](const std::string& virtual_file, std::string& line) {
      // drop trailing '\n'
      if (!line.empty())
        line.pop_back();
      CAF_CHECK_EQUAL(virtual_file, ":test");
      CAF_CHECK_EQUAL(line, chatty_line);
    }
  );
  self->await_all_other_actors_done();
  CAF_CHECK_EQUAL(self->mailbox().size(), 0u);
}

CAF_TEST(global_and_local_redirect) {
  self->join(system.groups().get_local(global_redirect));
  self->join(system.groups().get_local(local_redirect));
  actor_ostream::redirect_all(system, global_redirect);
  system.spawn(chatty_actor);
  system.spawn(chattier_actor, local_redirect);
  std::vector<std::pair<std::string, std::string>> expected {
    {":test", chatty_line},
    {":test", chatty_line},
    {":test2", chattier_line}
  };
  std::vector<std::pair<std::string, std::string>> lines;
  int i = 0;
  self->receive_for(i, 3)(
    [&](std::string& virtual_file, std::string& line) {
      // drop trailing '\n'
      if (!line.empty())
        line.pop_back();
      lines.emplace_back(std::move(virtual_file), std::move(line));
    }
  );
  CAF_CHECK(std::is_permutation(lines.begin(), lines.end(), expected.begin()));
  self->await_all_other_actors_done();
  CAF_CHECK_EQUAL(self->mailbox().size(), 0u);
}

CAF_TEST_FIXTURE_SCOPE_END()
