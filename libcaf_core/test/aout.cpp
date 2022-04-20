// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE aout

#include "core-test.hpp"

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

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(redirect_aout_globally) {
  self->join(system.groups().get_local(global_redirect));
  actor_ostream::redirect_all(system, global_redirect);
  system.spawn(chatty_actor);
  self->receive([](const std::string& virtual_file, std::string& line) {
    // drop trailing '\n'
    if (!line.empty())
      line.pop_back();
    CHECK_EQ(virtual_file, ":test");
    CHECK_EQ(line, chatty_line);
  });
  self->await_all_other_actors_done();
  CHECK_EQ(self->mailbox().size(), 0u);
}

CAF_TEST(global_and_local_redirect) {
  self->join(system.groups().get_local(global_redirect));
  self->join(system.groups().get_local(local_redirect));
  actor_ostream::redirect_all(system, global_redirect);
  system.spawn(chatty_actor);
  system.spawn(chattier_actor, local_redirect);
  std::vector<std::pair<std::string, std::string>> expected{
    {":test", chatty_line}, {":test", chatty_line}, {":test2", chattier_line}};
  std::vector<std::pair<std::string, std::string>> lines;
  int i = 0;
  self->receive_for(i, 3)([&](std::string& virtual_file, std::string& line) {
    // drop trailing '\n'
    if (!line.empty())
      line.pop_back();
    lines.emplace_back(std::move(virtual_file), std::move(line));
  });
  CHECK(std::is_permutation(lines.begin(), lines.end(), expected.begin()));
  self->await_all_other_actors_done();
  CHECK_EQ(self->mailbox().size(), 0u);
}

END_FIXTURE_SCOPE()
