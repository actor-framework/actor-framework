// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/actor_from_state.hpp"
#include "caf/anon_mail.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/log/test.hpp"

using namespace caf;

namespace {

behavior testee_impl(event_based_actor* self, actor buddy) {
  self->attach_functor([self, buddy](const error& rsn) {
    self->mail(ok_atom_v, rsn).send(buddy);
  });
  return {
    [self](delete_atom) {
      log::test::debug("testee received delete");
      self->quit(exit_reason::user_shutdown);
    },
  };
}

class spawner_state {
public:
  explicit spawner_state(event_based_actor* selfptr) : self(selfptr) {
    // nop
  }

  behavior make_behavior() {
    testee = self->spawn(testee_impl, actor{self});
    return {
      [this](ok_atom, const error& reason) {
        test::runnable::current().check_eq(reason, exit_reason::user_shutdown);
        self->quit(reason);
      },
      [this](delete_atom) {
        log::test::debug("spawner received delete");
        return self->mail(delete_atom_v).delegate(testee);
      },
    };
  }

  event_based_actor* self;
  actor testee;
};

WITH_FIXTURE(test::fixture::deterministic) {

TEST("constructor attach") {
  anon_mail(delete_atom_v).send(sys.spawn(actor_from_state<spawner_state>));
  dispatch_messages();
}

} // WITH_FIXTURE(test::fixture::deterministic)

} // namespace
