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

#define CAF_SUITE serial_reply
#include "caf/test/unit_test.hpp"

#include "caf/all.hpp"

using std::cout;
using std::endl;
using namespace caf;

CAF_TEST(test_serial_reply) {
  auto mirror_behavior = [=](event_based_actor* self) {
    self->become(others >> [=]() -> message {
      CAF_MESSAGE("return self->current_message()");
      return self->current_message();
    });
  };
  auto master = spawn([=](event_based_actor* self) {
    CAF_MESSAGE("ID of master: " << self->id());
    // spawn 5 mirror actors
    auto c0 = self->spawn<linked>(mirror_behavior);
    auto c1 = self->spawn<linked>(mirror_behavior);
    auto c2 = self->spawn<linked>(mirror_behavior);
    auto c3 = self->spawn<linked>(mirror_behavior);
    auto c4 = self->spawn<linked>(mirror_behavior);
    self->become (
      on(atom("hi there")) >> [=]() -> continue_helper {
      CAF_MESSAGE("received 'hi there'");
      return self->sync_send(c0, atom("sub0")).then(
        on(atom("sub0")) >> [=]() -> continue_helper {
        CAF_MESSAGE("received 'sub0'");
        return self->sync_send(c1, atom("sub1")).then(
          on(atom("sub1")) >> [=]() -> continue_helper {
          CAF_MESSAGE("received 'sub1'");
          return self->sync_send(c2, atom("sub2")).then(
            on(atom("sub2")) >> [=]() -> continue_helper {
            CAF_MESSAGE("received 'sub2'");
            return self->sync_send(c3, atom("sub3")).then(
              on(atom("sub3")) >> [=]() -> continue_helper {
              CAF_MESSAGE("received 'sub3'");
              return self->sync_send(c4, atom("sub4")).then(
                on(atom("sub4")) >> [=]() -> atom_value {
                CAF_MESSAGE("received 'sub4'");
                return atom("hiho");
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
    }
  );
  { // lifetime scope of self
    scoped_actor self;
    CAF_MESSAGE("ID of main: " << self->id());
    self->sync_send(master, atom("hi there")).await(
      on(atom("hiho")) >> [] {
        CAF_MESSAGE("received `hiho` atom");
      },
      others >> [&] {
        CAF_TEST_ERROR("Unexpected message: "
                       << to_string(self->current_message()));
      }
    );
    self->send_exit(master, exit_reason::user_shutdown);
  }
  await_all_actors_done();
  shutdown();
}
