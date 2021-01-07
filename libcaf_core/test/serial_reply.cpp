// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE serial_reply

#include "caf/all.hpp"

#include "core-test.hpp"

using namespace caf;

CAF_TEST(test_serial_reply) {
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
    CAF_MESSAGE("ID of master: " << self->id());
    // spawn 5 mirror actors
    auto c0 = self->spawn<linked>(mirror_behavior);
    auto c1 = self->spawn<linked>(mirror_behavior);
    auto c2 = self->spawn<linked>(mirror_behavior);
    auto c3 = self->spawn<linked>(mirror_behavior);
    auto c4 = self->spawn<linked>(mirror_behavior);
    return {
      [=](hi_atom) mutable {
        auto rp = self->make_response_promise();
        CAF_MESSAGE("received 'hi there'");
        self->request(c0, infinite, sub0_atom_v).then([=](sub0_atom) mutable {
          CAF_MESSAGE("received 'sub0'");
          self->request(c1, infinite, sub1_atom_v).then([=](sub1_atom) mutable {
            CAF_MESSAGE("received 'sub1'");
            self->request(c2, infinite, sub2_atom_v)
              .then([=](sub2_atom) mutable {
                CAF_MESSAGE("received 'sub2'");
                self->request(c3, infinite, sub3_atom_v)
                  .then([=](sub3_atom) mutable {
                    CAF_MESSAGE("received 'sub3'");
                    self->request(c4, infinite, sub4_atom_v)
                      .then([=](sub4_atom) mutable {
                        CAF_MESSAGE("received 'sub4'");
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
  CAF_MESSAGE("ID of main: " << self->id());
  self->request(master, infinite, hi_atom_v)
    .receive([](ho_atom) { CAF_MESSAGE("received 'ho'"); },
             [&](const error& err) { CAF_ERROR("Error: " << err); });
  CAF_REQUIRE(self->mailbox().empty());
}
