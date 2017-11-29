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

#define CAF_SUITE serial_reply
#include "caf/test/unit_test.hpp"

#include "caf/all.hpp"

using namespace caf;

namespace {

using hi_atom = atom_constant<atom("hi")>;
using ho_atom = atom_constant<atom("ho")>;
using sub0_atom = atom_constant<atom("sub0")>;
using sub1_atom = atom_constant<atom("sub1")>;
using sub2_atom = atom_constant<atom("sub2")>;
using sub3_atom = atom_constant<atom("sub3")>;
using sub4_atom = atom_constant<atom("sub4")>;

} // namespace <anonymous>


CAF_TEST(test_serial_reply) {
  actor_system_config cfg;
  actor_system system{cfg};
  auto mirror_behavior = [=](event_based_actor* self) -> behavior {
    self->set_default_handler(reflect);
    return {
      [] {
        // nop
      }
    };
  };
  auto master = system.spawn([=](event_based_actor* self) {
    CAF_MESSAGE("ID of master: " << self->id());
    // spawn 5 mirror actors
    auto c0 = self->spawn<linked>(mirror_behavior);
    auto c1 = self->spawn<linked>(mirror_behavior);
    auto c2 = self->spawn<linked>(mirror_behavior);
    auto c3 = self->spawn<linked>(mirror_behavior);
    auto c4 = self->spawn<linked>(mirror_behavior);
    self->become (
      [=](hi_atom) mutable {
        auto rp = self->make_response_promise();
        CAF_MESSAGE("received 'hi there'");
        self->request(c0, infinite, sub0_atom::value).then(
          [=](sub0_atom) mutable {
            CAF_MESSAGE("received 'sub0'");
            self->request(c1, infinite, sub1_atom::value).then(
              [=](sub1_atom) mutable {
                CAF_MESSAGE("received 'sub1'");
                self->request(c2, infinite, sub2_atom::value).then(
                  [=](sub2_atom) mutable {
                    CAF_MESSAGE("received 'sub2'");
                    self->request(c3, infinite, sub3_atom::value).then(
                      [=](sub3_atom) mutable {
                        CAF_MESSAGE("received 'sub3'");
                        self->request(c4, infinite, sub4_atom::value).then(
                          [=](sub4_atom) mutable {
                            CAF_MESSAGE("received 'sub4'");
                            rp.deliver(ho_atom::value);
                          }
                        );
                      }
                    );
                  }
                );
              }
            );
          }
        );
      }
    );
  });
  scoped_actor self{system};
  CAF_MESSAGE("ID of main: " << self->id());
  self->request(master, infinite, hi_atom::value).receive(
    [](ho_atom) {
      CAF_MESSAGE("received 'ho'");
    },
    [&](const error& err) {
      CAF_ERROR("Error: " << self->system().render(err));
    }
  );
  CAF_REQUIRE(self->mailbox().size() == 0);
}
