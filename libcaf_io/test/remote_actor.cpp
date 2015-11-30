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

#define CAF_SUITE io_dynamic_remote_actor
#include "caf/test/unit_test.hpp"

#include <thread>
#include <string>
#include <cstring>
#include <sstream>
#include <iostream>
#include <functional>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

using namespace std;
using namespace caf;

namespace {

using spawn5_done_atom = atom_constant<atom("Spawn5Done")>;
using spawn_ping_atom = atom_constant<atom("SpawnPing")>;
using get_group_atom = atom_constant<atom("GetGroup")>;
using sync_msg_atom = atom_constant<atom("SyncMsg")>;
using ping_ptr_atom = atom_constant<atom("PingPtr")>;
using gclient_atom = atom_constant<atom("GClient")>;
using spawn5_atom = atom_constant<atom("Spawn5")>;
using ping_atom = atom_constant<atom("ping")>;
using pong_atom = atom_constant<atom("pong")>;
using foo_atom = atom_constant<atom("foo")>;
using bar_atom = atom_constant<atom("bar")>;

atomic<long> s_destructors_called;
atomic<long> s_on_exit_called;

constexpr size_t num_pings = 10;

size_t s_pongs = 0;

behavior ping_behavior(local_actor* self, size_t ping_msgs) {
  CAF_LOG_TRACE(CAF_ARG(ping_msgs));
  return {
    [=](pong_atom, int value) -> message {
      CAF_LOG_TRACE(CAF_ARG(value));
      if (! self->current_sender()) {
        CAF_TEST_ERROR("current_sender() invalid!");
      }
      CAF_MESSAGE("received {'pong', " << value << "}");
      if (++s_pongs >= ping_msgs) {
        CAF_MESSAGE("reached maximum, send {'EXIT', user_defined} "
                      << "to last sender and quit with normal reason");
        self->send_exit(self->current_sender(),
                exit_reason::user_shutdown);
        self->quit();
      }
      return make_message(ping_atom::value, value);
    },
    others() >> [=] {
      CAF_LOG_TRACE("");
      self->quit(exit_reason::user_shutdown);
    }
  };
}

behavior pong_behavior(local_actor* self) {
  CAF_LOG_TRACE("");
  return {
    [](ping_atom, int value)->message {
      CAF_LOG_TRACE(CAF_ARG(value));
      return make_message(pong_atom::value, value + 1);
    },
    others() >> [=] {
      CAF_LOG_TRACE("");
      self->quit(exit_reason::user_shutdown);
    }
  };
}

size_t pongs() {
  return s_pongs;
}

void event_based_ping(event_based_actor* self, size_t ping_msgs) {
  CAF_LOG_TRACE(CAF_ARG(ping_msgs));
  s_pongs = 0;
  self->become(ping_behavior(self, ping_msgs));
}

void pong(blocking_actor* self, actor ping_actor) {
  CAF_LOG_TRACE("");
  self->send(ping_actor, pong_atom::value, 0); // kickoff
  self->receive_loop(pong_behavior(self));
}

using string_pair = std::pair<std::string, std::string>;

using actor_vector = vector<actor>;

void reflector(event_based_actor* self) {
  CAF_LOG_TRACE("");
  self->become(
    others >> [=] {
      CAF_LOG_TRACE("");
      CAF_MESSAGE("reflect and quit; sender was: "
                  << to_string(self->current_sender()));
      self->quit();
      return self->current_message();
  });
}

void spawn5_server_impl(event_based_actor* self, actor client, group grp) {
  CAF_LOG_TRACE("");
  CAF_CHECK(grp != invalid_group);
  for (int i = 0; i < 2; ++i)
    CAF_MESSAGE("spawned local subscriber: "
                << self->spawn_in_group(grp, reflector)->id());
  CAF_MESSAGE("send {'Spawn5'} and await {'ok', actor_vector}");
  self->sync_send(client, spawn5_atom::value, grp).then(
    [=](ok_atom, const actor_vector& vec) {
      CAF_LOG_TRACE(CAF_ARG(vec));
      CAF_MESSAGE("received vector with " << vec.size() << " elements");
      auto is_remote = [self](const actor& x) -> bool {
        return self->node() != x.node();
      };
      CAF_CHECK(std::all_of(vec.begin(), vec.end(), is_remote));
      self->send(grp, "Hello reflectors!", 5.0);
      if (vec.size() != 5) {
        CAF_MESSAGE("remote client did not spawn five reflectors!");
      }
      for (auto& a : vec) {
        CAF_MESSAGE("monitor actor: " << to_string(a));
        self->monitor(a);
      }
      CAF_MESSAGE("wait for reflected messages");
      // receive seven reply messages (2 local, 5 remote)
      auto replies = std::make_shared<int>(0);
      self->become(
        [=](const std::string& x0, double x1) {
          CAF_MESSAGE((self->node() == self->current_sender()->node()
                       ? "local" : "remote")
                      << " answer from " << to_string(self->current_sender()));
          CAF_CHECK_EQUAL(x0, "Hello reflectors!");
          CAF_CHECK_EQUAL(x1, 5.0);
          if (++*replies == 7) {
            CAF_MESSAGE("wait for DOWN messages");
            auto downs = std::make_shared<int>(0);
            self->become(
              [=](const down_msg& dm) {
                if (dm.reason != exit_reason::normal) {
                  CAF_TEST_ERROR("reflector exited for non-normal exit reason!");
                }
                if (++*downs == 5) {
                  CAF_MESSAGE("down increased to 5, about to quit");
                  self->send(client, spawn5_done_atom::value);
                  self->quit();
                }
              },
              others >> [=] {
                CAF_TEST_ERROR("Unexpected message");
                self->quit(exit_reason::user_defined);
              },
              after(chrono::seconds(3)) >> [=] {
                CAF_TEST_ERROR("did only receive " << *downs << " down messages");
                self->quit(exit_reason::user_defined);
              }
            );
          }
        },
        after(std::chrono::seconds(6)) >> [=] {
          CAF_LOG_TRACE("");
          CAF_TEST_ERROR("Unexpected timeout");
          self->quit(exit_reason::user_defined);
        }
      );
    },
    others >> [=] {
      CAF_LOG_TRACE("");
      CAF_TEST_ERROR("Unexpected message");
      self->quit(exit_reason::user_defined);
    },
    after(chrono::seconds(10)) >> [=] {
      CAF_LOG_TRACE("");
      CAF_TEST_ERROR("Unexpected timeout");
      self->quit(exit_reason::user_defined);
    }
  );
}

// receive seven reply messages (2 local, 5 remote)
void spawn5_server(event_based_actor* self, actor client, bool inverted) {
  CAF_LOG_TRACE("");
  CAF_REQUIRE(self->node() != client.node());
  CAF_MESSAGE("spawn5_server, inverted: " << inverted);
  if (! inverted) {
    spawn5_server_impl(self, client, self->system().groups().get("local",
                                                                 "foobar"));
  } else {
    CAF_MESSAGE("request group");
    self->sync_send(client, get_group_atom::value).then(
      [=](const group& remote_group) {
        CAF_LOG_TRACE(CAF_ARG(remote_group));
        CAF_REQUIRE(remote_group != invalid_group);
        CAF_REQUIRE(self->current_sender() != invalid_actor_addr);
        CAF_CHECK(self->node() != self->current_sender()->node());
        CAF_MESSAGE("got group: " << to_string(remote_group)
                    << " from " << to_string(self->current_sender()));
        spawn5_server_impl(self, client, remote_group);
      }
    );
  }
}

void spawn5_client(event_based_actor* self) {
  CAF_LOG_TRACE("");
  self->become(
    [=](get_group_atom) -> group {
      CAF_LOG_TRACE("");
      CAF_MESSAGE("received {'GetGroup'}");
      return self->system().groups().get("local", "foobar");
    },
    [=](spawn5_atom, const group& grp) -> message {
      CAF_LOG_TRACE(CAF_ARG(grp));
      CAF_REQUIRE(grp != invalid_group);
      CAF_MESSAGE("received: " << to_string(self->current_message()));
      actor_vector vec;
      for (int i = 0; i < 5; ++i)
        vec.push_back(self->system().spawn_in_group(grp, reflector));
      CAF_MESSAGE("spawned all reflectors");
      return make_message(ok_atom::value, std::move(vec));
    },
    [=](spawn5_done_atom) {
      CAF_LOG_TRACE("");
      CAF_MESSAGE("received {'Spawn5Done'}");
      self->quit();
    }
  );
}

template <class F>
void await_down(event_based_actor* self, actor ptr, F continuation) {
  CAF_LOG_TRACE(CAF_ARG(ptr));
  self->become(
    [=](const down_msg& dm) -> maybe<skip_message_t> {
      CAF_LOG_TRACE(CAF_ARG(dm));
      if (dm.source == ptr) {
        continuation();
        return none;
      }
      return skip_message(); // not the 'DOWN' message we are waiting for
    }
  );
}

class client : public event_based_actor {
public:
  client(actor_config& cfg, actor server)
      : event_based_actor(cfg),
        server_(std::move(server)) {
    CAF_LOG_TRACE(CAF_ARG(server));
  }

  behavior make_behavior() override {
    CAF_LOG_TRACE("");
    return spawn_ping();
  }

  void on_exit() override {
    ++s_on_exit_called;
  }

  ~client() {
    ++s_destructors_called;
  }

private:
  behavior spawn_ping() {
    CAF_LOG_TRACE("");
    CAF_MESSAGE("send {'SpawnPing'}");
    send(server_, spawn_ping_atom::value);
    return {
      [=](ping_ptr_atom, const actor& ping) {
        CAF_LOG_TRACE(CAF_ARG(ping));
        CAF_MESSAGE("received ping pointer, spawn pong");
        auto pptr = spawn<monitored + detached + blocking_api>(pong, ping);
        await_down(this, pptr, [=] { send_sync_msg(); });
      }
    };
  }

  void send_sync_msg() {
    CAF_LOG_TRACE("");
    CAF_MESSAGE("sync send {'SyncMsg', 4.2f}");
    sync_send(server_, sync_msg_atom::value, 4.2f).then(
      [=](ok_atom) {
        CAF_LOG_TRACE("");
        send_foobars();
      }
    );
  }

  void send_foobars(int i = 0) {
    CAF_LOG_TRACE("");
    if (i == 0) {
      CAF_MESSAGE("send foobars");
    }
    if (i == 100)
      test_group_comm();
    else {
      sync_send(server_, foo_atom::value, bar_atom::value, i).then(
        [=](foo_atom, bar_atom, int res) {
          CAF_LOG_TRACE(CAF_ARG(res));
          CAF_CHECK_EQUAL(res, i);
          send_foobars(i + 1);
        }
      );
    }
  }

  void test_group_comm() {
    CAF_LOG_TRACE("");
    CAF_MESSAGE("test group communication via network");
    sync_send(server_, gclient_atom::value).then(
      [=](gclient_atom, actor gclient) {
        CAF_LOG_TRACE(CAF_ARG(gclient));
        auto s5a = spawn<monitored>(spawn5_server, gclient, false);
        await_down(this, s5a, [=] { test_group_comm_inverted(); });
      }
    );
  }

  void test_group_comm_inverted() {
    CAF_LOG_TRACE("");
    CAF_MESSAGE("test group communication via network (inverted setup)");
    become(
      [=](gclient_atom) -> message {
        CAF_LOG_TRACE("");
        CAF_MESSAGE("received `gclient_atom`");
        auto cptr = current_sender();
        auto s5c = spawn<monitored>(spawn5_client);
        // set next behavior
        await_down(this, s5c, [=] {
          CAF_LOG_TRACE("");
          CAF_MESSAGE("set next behavior");
          quit();
        });
        return make_message(gclient_atom::value, s5c);
      }
    );
  }

  actor server_;
};

class server : public event_based_actor {
public:
  server(actor_config& cfg, bool run_in_loop = false)
      : event_based_actor(cfg),
        run_in_loop_(run_in_loop) {
    CAF_LOG_TRACE(CAF_ARG(run_in_loop));
  }

  behavior make_behavior() override {
    CAF_LOG_TRACE("");
    if (run_in_loop_)
      trap_exit(true);
    return await_spawn_ping();
  }

  void on_exit() override {
    ++s_on_exit_called;
  }

  ~server() {
    ++s_destructors_called;
  }

private:
  behavior await_spawn_ping() {
    CAF_LOG_TRACE("");
    CAF_MESSAGE("await {'SpawnPing'}");
    return {
      [=](spawn_ping_atom) -> message {
        CAF_LOG_TRACE("");
        CAF_MESSAGE("received {'SpawnPing'}");
        auto client = current_sender();
        if (! client) {
          CAF_MESSAGE("last_sender() invalid!");
        }
        CAF_MESSAGE("spawn event-based ping actor");
        auto pptr = spawn<monitored>(event_based_ping, num_pings);
        CAF_MESSAGE("wait until spawned ping actor is done");
        await_down(this, pptr, [=] {
          CAF_CHECK_EQUAL(pongs(), num_pings);
          become(await_sync_msg());
        });
        return make_message(ping_ptr_atom::value, pptr);
      },
      [](const exit_msg&) {
        CAF_LOG_TRACE("");
        // simply ignored if trap_exit is true
      }
    };
  }

  behavior await_sync_msg() {
    CAF_LOG_TRACE("");
    CAF_MESSAGE("await {'SyncMsg'}");
    return {
      [=](sync_msg_atom, float f) -> atom_value {
        CAF_LOG_TRACE("");
        CAF_MESSAGE("received: " << to_string(current_message()));
        CAF_CHECK_EQUAL(f, 4.2f);
        become(await_foobars());
        return ok_atom::value;
      },
      [](const exit_msg&) {
        CAF_LOG_TRACE("");
        // simply ignored if trap_exit is true
      }
    };
  }

  behavior await_foobars() {
    CAF_LOG_TRACE("");
    CAF_MESSAGE("await foobars");
    auto foobars = make_shared<int>(0);
    return {
      [=](foo_atom, bar_atom, int i) -> message {
        CAF_LOG_TRACE(CAF_ARG(i));
        ++*foobars;
        if (i == 99) {
          CAF_CHECK_EQUAL(*foobars, 100);
          become(test_group_comm());
        }
        return std::move(current_message());
      },
      [](const exit_msg&) {
        // simply ignored if trap_exit is true
      }
    };
  }

  behavior test_group_comm() {
    CAF_LOG_TRACE("");
    CAF_MESSAGE("test group communication via network");
    return {
      [=](gclient_atom) -> message {
        CAF_LOG_TRACE("");
        CAF_MESSAGE("received `gclient_atom`");
        auto cptr = current_sender();
        auto s5c = spawn<monitored>(spawn5_client);
        await_down(this, s5c, [=] {
          CAF_MESSAGE("test_group_comm_inverted");
          test_group_comm_inverted(actor_cast<actor>(cptr));
        });
        return make_message(gclient_atom::value, s5c);
      },
      [](const exit_msg&) {
        CAF_LOG_TRACE("");
        // simply ignored if trap_exit is true
      }
    };
  }

  void test_group_comm_inverted(actor cptr) {
    CAF_LOG_TRACE("");
    CAF_MESSAGE("test group communication via network (inverted setup)");
    sync_send(cptr, gclient_atom::value).then(
      [=](gclient_atom, actor gclient) {
        CAF_LOG_TRACE(CAF_ARG(gclient));
        await_down(this, spawn<monitored>(spawn5_server, gclient, true), [=] {
          CAF_LOG_TRACE("");
          CAF_MESSAGE("`await_down` finished");
          if (! run_in_loop_)
            quit();
          else
            become(await_spawn_ping());
        });
      }
    );
  }

  bool run_in_loop_;
};

void launch_remote_side(int argc, char** argv, uint16_t group_port,
                        uint16_t client_port1, uint16_t client_port2) {
  actor_system_config cfg{argc, argv};
  cfg.load<io::middleman>()
     .add_message_type<actor_vector>("actor_vector");
  CAF_LOG_TRACE(CAF_ARG(group_port) << CAF_ARG(client_port1)
                << CAF_ARG(client_port2));
  CAF_MESSAGE("launch_remote_side(" << group_port
              << ", " << client_port1 << ", " << client_port2 << ")");
  actor_system system{cfg};
  scoped_actor self{system, true};
  auto serv = system.middleman().remote_actor("127.0.0.1", client_port1);
  auto serv2 = system.middleman().remote_actor("127.0.0.1", client_port2);
  CAF_REQUIRE(serv);
  // remote_actor is supposed to return the same server
  // when connecting to the same host again
  CAF_CHECK(serv == system.middleman().remote_actor("127.0.0.1", client_port1));
  CAF_CHECK(serv2 == system.middleman().remote_actor("127.0.0.1", client_port2));
  // connect to published groups
  auto grp = system.middleman().remote_group("whatever", "127.0.0.1", group_port);
  auto c = self->spawn<client, monitored>(serv);
  self->receive(
    [&](const down_msg& dm) {
      CAF_CHECK(dm.source == c);
      CAF_CHECK_EQUAL(dm.reason, exit_reason::normal);
    }
  );
  system.await_all_actors_done();
}

void test_remote_actor(int argc, char** argv) {
  actor_system_config cfg{argc, argv};
  cfg.load<io::middleman>()
     .add_message_type<actor_vector>("actor_vector");
  actor_system system{cfg};
  scoped_actor self{system, true};
  auto serv = self->spawn<server, monitored>();
  // publish on two distinct ports and use the latter one afterwards
  auto port1 = system.middleman().publish(serv, 0, "127.0.0.1");
  CAF_REQUIRE(port1 && *port1 > 0);
  CAF_MESSAGE("first publish succeeded on port " << *port1);
  auto port2 = system.middleman().publish(serv, 0, "127.0.0.1");
  CAF_REQUIRE(port2 && *port2 > 0);
  CAF_MESSAGE("second publish succeeded on port " << *port2);
  // publish local groups as well
  auto gport = system.middleman().publish_local_groups(0);
  CAF_REQUIRE(gport && *gport > 0);
  CAF_MESSAGE("local groups published on port " << *gport);
  // check whether accessing local actors via system.middleman().remote_actors
  // works correctly, i.e., does not return a proxy instance
  auto serv2 = system.middleman().remote_actor("127.0.0.1", *port2);
  CAF_CHECK(serv2 && system.node() != serv2->node());
  CAF_CHECK(serv == serv2);
  launch_remote_side(argc, argv, *gport, *port1, *port2);
  self->receive(
    [&](const down_msg& dm) {
      CAF_LOG_TRACE(CAF_ARG(dm));
      CAF_CHECK(dm.source == serv);
      CAF_CHECK_EQUAL(dm.reason, exit_reason::normal);
    }
  );
  CAF_MESSAGE("wait for other actors");
  self->await_all_other_actors_done();
}

} // namespace <anonymous>

CAF_TEST(remote_actors) {
  auto argv = test::engine::argv();
  auto argc = test::engine::argc();
  test_remote_actor(argc, argv);
  // check after dtor of actor system ran
  // we spawn a server and a client, in both cases
  // there must have been exactly one dtor called (two in total)
  CAF_CHECK_EQUAL(s_destructors_called.load(), 2);
  CAF_CHECK_EQUAL(s_on_exit_called.load(), 2);
}
