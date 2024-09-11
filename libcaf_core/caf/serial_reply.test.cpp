// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/test/test.hpp"

#include "caf/actor_system_config.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/init_global_meta_objects.hpp"
#include "caf/log/test.hpp"
#include "caf/scoped_actor.hpp"

using namespace caf;

CAF_BEGIN_TYPE_ID_BLOCK(serial_reply_test, caf::first_custom_type_id + 30)

  CAF_ADD_ATOM(serial_reply_test, sub0_atom)
  CAF_ADD_ATOM(serial_reply_test, sub1_atom)
  CAF_ADD_ATOM(serial_reply_test, sub2_atom)
  CAF_ADD_ATOM(serial_reply_test, sub3_atom)
  CAF_ADD_ATOM(serial_reply_test, sub4_atom)
  CAF_ADD_ATOM(serial_reply_test, hi_atom)
  CAF_ADD_ATOM(serial_reply_test, ho_atom)

CAF_END_TYPE_ID_BLOCK(serial_reply_test)

TEST("test_serial_reply") {
  actor_system_config cfg;
  actor_system system{cfg};
  auto mirror_behavior = [=](event_based_actor* self) -> behavior {
    self->set_default_handler(reflect);
    return {
      [] {
        // nop
      },
    };
  };
  auto master = system.spawn([=](event_based_actor* self) -> behavior {
    log::test::debug("ID of master: {}", self->id());
    // spawn 5 mirror actors
    auto c0 = self->spawn<linked>(mirror_behavior);
    auto c1 = self->spawn<linked>(mirror_behavior);
    auto c2 = self->spawn<linked>(mirror_behavior);
    auto c3 = self->spawn<linked>(mirror_behavior);
    auto c4 = self->spawn<linked>(mirror_behavior);
    return {
      [=](hi_atom) mutable {
        auto rp = self->make_response_promise();
        log::test::debug("received 'hi there'");
        self->mail(sub0_atom_v)
          .request(c0, infinite)
          .then([=](sub0_atom) mutable {
            log::test::debug("received 'sub0'");
            self->mail(sub1_atom_v)
              .request(c1, infinite)
              .then([=](sub1_atom) mutable {
                log::test::debug("received 'sub1'");
                self->mail(sub2_atom_v)
                  .request(c2, infinite)
                  .then([=](sub2_atom) mutable {
                    log::test::debug("received 'sub2'");
                    self->mail(sub3_atom_v)
                      .request(c3, infinite)
                      .then([=](sub3_atom) mutable {
                        log::test::debug("received 'sub3'");
                        self->mail(sub4_atom_v)
                          .request(c4, infinite)
                          .then([=](sub4_atom) mutable {
                            log::test::debug("received 'sub4'");
                            rp.deliver(ho_atom_v);
                          });
                      });
                  });
              });
          });
      },
    };
  });
  scoped_actor self{system};
  log::test::debug("ID of main: {}", self->id());
  self->mail(hi_atom_v)
    .request(master, infinite)
    .receive([](ho_atom) { log::test::debug("received 'ho'"); },
             [this](const error& err) { return fail("Error: {}", err); });
  require(self->mailbox().empty());
}

TEST_INIT() {
  init_global_meta_objects<id_block::serial_reply_test>();
}
