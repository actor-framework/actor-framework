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

#define CAF_SUITE local_migration
#include "caf/test/unit_test.hpp"

#include "caf/all.hpp"

#include "caf/detail/actor_registry.hpp"

using namespace caf;

using std::endl;

namespace {

struct migratable_state {
  int value = 0;
  static const char* name;
};

const char* migratable_state::name = "migratable_actor";

template <class Archive>
void serialize(Archive& ar, migratable_state& x, const unsigned int) {
  ar & x.value;
}

struct migratable_actor : stateful_actor<migratable_state> {
  behavior make_behavior() override {
    return {
      [=](get_atom) {
        return state.value;
      },
      [=](put_atom, int value) {
        state.value = value;
      }
    };
  }
};

// always migrates to `dest`
behavior pseudo_mm(event_based_actor* self, const actor& dest) {
  return {
    [=](migrate_atom, const std::string& name, std::vector<char>& buf) {
      CAF_CHECK(name == "migratable_actor");
      self->delegate(dest, sys_atom::value, migrate_atom::value,
                     std::move(buf));
    }
  };
}

} // namespace <anonymous>

CAF_TEST(migrate_locally) {
  auto a = spawn<migratable_actor>();
  auto b = spawn<migratable_actor>();
  auto mm1 = spawn(pseudo_mm, b);
  { // Lifetime scope of scoped_actor
    scoped_actor self;
    self->send(a, put_atom::value, 42);
    // migrate from a to b
    self->sync_send(a, sys_atom::value, migrate_atom::value, mm1).await(
      [&](ok_atom, const actor_addr& dest) {
        CAF_CHECK(dest == b);
      }
    );
    self->sync_send(a, get_atom::value).await(
      [&](int result) {
        CAF_CHECK(result == 42);
        CAF_CHECK(self->current_sender() == b.address());
      }
    );
    auto mm2 = spawn(pseudo_mm, a);
    self->send(b, put_atom::value, 23);
    // migrate back from b to a
    self->sync_send(b, sys_atom::value, migrate_atom::value, mm2).await(
      [&](ok_atom, const actor_addr& dest) {
        CAF_CHECK(dest == a);
      }
    );
    self->sync_send(b, get_atom::value).await(
      [&](int result) {
        CAF_CHECK(result == 23);
        CAF_CHECK(self->current_sender() == a.address());
      }
    );
    self->send_exit(a, exit_reason::kill);
    self->send_exit(b, exit_reason::kill);
    self->send_exit(mm1, exit_reason::kill);
    self->send_exit(mm2, exit_reason::kill);
    self->await_all_other_actors_done();
  }
  shutdown();
}
