#include <thread>
#include <string>
#include <cstring>
#include <sstream>
#include <iostream>
#include <functional>

#include "test.hpp"
#include "ping_pong.hpp"

#include "caf/all.hpp"
#include "caf/io/all.hpp"

#include "caf/detail/logging.hpp"
#include "caf/detail/singletons.hpp"

using namespace std;
using namespace caf;

namespace {

atomic<long> s_destructors_called;

using string_pair = std::pair<std::string, std::string>;

using actor_vector = vector<actor>;

void reflector(event_based_actor* self) {
  self->become(others() >> [=] {
    CAF_PRINT("reflect and quit");
    self->quit();
    return self->last_dequeued();
  });
}

void spawn5_server_impl(event_based_actor* self, actor client, group grp) {
  CAF_LOGF_TRACE(CAF_TARG(client, to_string)
                 << ", " << CAF_TARG(grp, to_string));
  CAF_CHECK(grp != invalid_group);
  self->spawn_in_group(grp, reflector);
  self->spawn_in_group(grp, reflector);
  CAF_PRINT("send {'Spawn5'} and await {'ok', actor_vector}");
  self->sync_send(client, atom("Spawn5"), grp).then(
    on(atom("ok"), arg_match) >> [=](const actor_vector& vec) {
      CAF_PRINT("received vector with " << vec.size() << " elements");
      self->send(grp, "Hello reflectors!", 5.0);
      if (vec.size() != 5) {
        CAF_PRINTERR("remote client did not spawn five reflectors!");
      }
      for (auto& a : vec) {
        CAF_PRINT("monitor actor: " << to_string(a));
        self->monitor(a);
      }
    },
    others() >> [=] {
      CAF_UNEXPECTED_MSG(self);
      self->quit(exit_reason::user_defined);
    },
    after(chrono::seconds(10)) >> [=] {
      CAF_UNEXPECTED_TOUT();
       self->quit(exit_reason::user_defined);
     }
  ).continue_with([=] {
    CAF_PRINT("wait for reflected messages");
    // receive seven reply messages (2 local, 5 remote)
    auto replies = std::make_shared<int>(0);
    self->become(
      on("Hello reflectors!", 5.0) >> [=] {
        if (++*replies == 7) {
          CAF_PRINT("wait for DOWN messages");
          auto downs = std::make_shared<int>(0);
          self->become(
            [=](const down_msg& dm) {
              if (dm.reason != exit_reason::normal) {
                CAF_PRINTERR("reflector exited for non-normal exit reason!");
              }
              if (++*downs == 5) {
                CAF_CHECKPOINT();
                self->send(client, atom("Spawn5Done"));
                self->quit();
              }
            },
            others() >> [=] {
              CAF_UNEXPECTED_MSG(self);
              self->quit(exit_reason::user_defined);
            },
            after(chrono::seconds(2)) >> [=] {
              CAF_UNEXPECTED_TOUT();
              CAF_LOGF_ERROR("did only receive " << *downs << " down messages");
              self->quit(exit_reason::user_defined);
            }
          );
        }
      },
      after(std::chrono::seconds(2)) >> [=] {
        CAF_UNEXPECTED_TOUT();
        CAF_LOGF_ERROR("did only receive " << *replies
                       << " responses to 'Hello reflectors!'");
        self->quit(exit_reason::user_defined);
      }
    );
 });
}

// receive seven reply messages (2 local, 5 remote)
void spawn5_server(event_based_actor* self, actor client, bool inverted) {
  if (!inverted)
    spawn5_server_impl(self, client, group::get("local", "foobar"));
  else {
    CAF_PRINT("request group");
    self->sync_send(client, atom("GetGroup"))
      .then([=](const group& remote_group) {
         spawn5_server_impl(self, client, remote_group);
       });
  }
}

void spawn5_client(event_based_actor* self) {
  self->become(
    on(atom("GetGroup")) >> []()->group {
      CAF_PRINT("received {'GetGroup'}");
      return group::get("local", "foobar");
    },
    on(atom("Spawn5"), arg_match) >> [=](const group & grp)->message {
      CAF_PRINT("received {'Spawn5'}");
      actor_vector vec;
      for (int i = 0; i < 5; ++i) {
        CAF_CHECKPOINT();
        vec.push_back(spawn_in_group(grp, reflector));
      }
      CAF_CHECKPOINT();
      return make_message(atom("ok"), std::move(vec));
    },
    on(atom("Spawn5Done")) >> [=] {
      CAF_PRINT("received {'Spawn5Done'}");
      self->quit();
    });
}

template <class F>
void await_down(event_based_actor* self, actor ptr, F continuation) {
  self->become(
    [=](const down_msg & dm) -> bool {
      if (dm.source == ptr) {
        continuation();
        return true;
      }
      return false; // not the 'DOWN' message we are waiting for
    }
  );
}

static constexpr size_t num_pings = 10;

class client : public event_based_actor {

 public:

  client(actor server) : m_server(std::move(server)) {
    // nop
  }

  behavior make_behavior() override {
    return spawn_ping();
  }

  ~client() {
    ++s_destructors_called;
  }

 private:

  behavior spawn_ping() {
    CAF_PRINT("send {'SpawnPing'}");
    send(m_server, atom("SpawnPing"));
    return {
      on(atom("PingPtr"), arg_match) >> [=](const actor& ping) {
        CAF_PRINT("received ping pointer, spawn pong");
        auto pptr = spawn<monitored + detached + blocking_api>(pong, ping);
        await_down(this, pptr, [=] { send_sync_msg(); });
      }
    };
  }

  void send_sync_msg() {
    CAF_PRINT("sync send {'SyncMsg', 4.2fSyncMsg}");
    sync_send(m_server, atom("SyncMsg"), 4.2f)
      .then(on(atom("SyncReply")) >> [=] { send_foobars(); });
  }

  void send_foobars(int i = 0) {
    if (i == 0) {
      CAF_PRINT("send foobars");
    }
    if (i == 100)
      test_group_comm();
    else {
      CAF_LOG_DEBUG("send message nr. " << (i + 1));
      sync_send(m_server, atom("foo"), atom("bar"), i)
        .then(on(atom("foo"), atom("bar"), i) >> [=] {
           send_foobars(i + 1);
         });
    }
  }

  void test_group_comm() {
    CAF_PRINT("test group communication via network");
    sync_send(m_server, atom("GClient"))
      .then(on(atom("GClient"), arg_match) >> [=](actor gclient) {
         CAF_CHECKPOINT();
         auto s5a = spawn<monitored>(spawn5_server, gclient, false);
         await_down(this, s5a, [=] { test_group_comm_inverted(); });
       });
  }

  void test_group_comm_inverted() {
    CAF_PRINT("test group communication via network (inverted setup)");
    become(on(atom("GClient")) >> [=]()->message {
      CAF_CHECKPOINT();
      auto cptr = last_sender();
      auto s5c = spawn<monitored>(spawn5_client);
      // set next behavior
      await_down(this, s5c, [=] {
        CAF_CHECKPOINT();
        quit();
      });
      return make_message(atom("GClient"), s5c);
    });
  }

  actor m_server;

};

class server : public event_based_actor {

 public:

  behavior make_behavior() override {
    return await_spawn_ping();
  }

  server(bool run_in_loop = false) : m_run_in_loop(run_in_loop) {
    // nop
  }

  ~server() {
    ++s_destructors_called;
  }

 private:

  behavior await_spawn_ping() {
    CAF_PRINT("await {'SpawnPing'}");
    return (on(atom("SpawnPing")) >> [=]()->message {
      CAF_PRINT("received {'SpawnPing'}");
      auto client = last_sender();
      if (!client) {
        CAF_PRINT("last_sender() invalid!");
      }
      CAF_PRINT("spawn event-based ping actor");
      auto pptr = spawn<monitored>(event_based_ping, num_pings);
      CAF_PRINT("wait until spawned ping actor is done");
      await_down(this, pptr, [=] {
        CAF_CHECK_EQUAL(pongs(), num_pings);
        await_sync_msg();
      });
      return make_message(atom("PingPtr"), pptr);
    });
  }

  void await_sync_msg() {
    CAF_PRINT("await {'SyncMsg'}");
    become(on(atom("SyncMsg"), arg_match) >> [=](float f)->atom_value {
      CAF_PRINT("received {'SyncMsg', " << f << "}");
      CAF_CHECK_EQUAL(f, 4.2f);
      await_foobars();
      return atom("SyncReply");
    });
  }

  void await_foobars() {
    CAF_PRINT("await foobars");
    auto foobars = make_shared<int>(0);
    become(on(atom("foo"), atom("bar"), arg_match) >> [=](int i)->message {
      ++*foobars;
      if (i == 99) {
        CAF_CHECK_EQUAL(*foobars, 100);
        test_group_comm();
      }
      return last_dequeued();
    });
  }

  void test_group_comm() {
    CAF_PRINT("test group communication via network");
    become(on(atom("GClient")) >> [=]()->message {
      CAF_CHECKPOINT();
      auto cptr = last_sender();
      auto s5c = spawn<monitored>(spawn5_client);
      await_down(this, s5c, [=] {
        CAF_CHECKPOINT();
        test_group_comm_inverted(actor_cast<actor>(cptr));
      });
      return make_message(atom("GClient"), s5c);
    });
  }

  void test_group_comm_inverted(actor cptr) {
    CAF_PRINT("test group communication via network (inverted setup)");
    sync_send(cptr, atom("GClient")).then(
      on(atom("GClient"), arg_match) >> [=](actor gclient) {
         await_down(this,
              spawn<monitored>(spawn5_server, gclient, true),
              [=] {
                CAF_CHECKPOINT();
                if (!m_run_in_loop) {
                  quit();
                } else {
                  become(await_spawn_ping());
                }
              });
       }
    );
  }

  bool m_run_in_loop;

};

template <class F>
uint16_t at_some_port(uint16_t first_port, F fun) {
  auto port = first_port;
  for (;;) {
    try {
      fun(port);
      return port;
    }
    catch (bind_failure&) {
      // try next port
      ++port;
    }
  }
}

void test_remote_actor(std::string app_path, bool run_remote_actor) {
  scoped_actor self;
  auto serv = self->spawn<server, monitored>();
  auto publish_serv = [=](uint16_t p) {
    io::publish(serv, p, "127.0.0.1");
  };
  auto publish_groups = [](uint16_t p) {
    io::publish_local_groups(p);
  };
  // publish on two distinct ports and use the latter one afterwards
  auto port0 = at_some_port(4242, publish_serv);
  CAF_LOGF_INFO("first publish succeeded on port " << port0);
  auto port = at_some_port(port0 + 1, publish_serv);
  CAF_PRINT("running on port " << port);
  CAF_LOGF_INFO("running on port " << port);
  // publish local groups as well
  auto gport = at_some_port(port + 1, publish_groups);
  // check whether accessing local actors via io::remote_actors works correctly,
  // i.e., does not return a proxy instance
  auto serv2 = io::remote_actor("127.0.0.1", port);
  CAF_CHECK(serv2 != invalid_actor && !serv2->is_remote());
  CAF_CHECK(serv == serv2);
  thread child;
  ostringstream oss;
  oss << app_path << " -c " << port << " " << port0 << " " << gport;
  if (run_remote_actor) {
    oss << to_dev_null;
    // execute client_part() in a separate process,
    // connected via localhost socket
    child = thread([&oss]() {
      CAF_LOGC_TRACE("NONE", "main$thread_launcher", "");
      string cmdstr = oss.str();
      if (system(cmdstr.c_str()) != 0) {
        CAF_PRINTERR("FATAL: command \"" << cmdstr << "\" failed!");
        abort();
      }
    });
  } else {
    CAF_PRINT("please run client: " << oss.str());
  }
  CAF_CHECKPOINT();
  self->receive(
    [&](const down_msg& dm) {
      CAF_CHECK_EQUAL(dm.source, serv);
      CAF_CHECK_EQUAL(dm.reason, exit_reason::normal);
    }
  );
  // wait until separate process (in sep. thread) finished execution
  CAF_CHECKPOINT();
  if (run_remote_actor) child.join();
  CAF_CHECKPOINT();
  self->await_all_other_actors_done();
}

} // namespace <anonymous>

int main(int argc, char** argv) {
  CAF_TEST(test_remote_actor);
  announce<actor_vector>();
  cout << "this node is: " << to_string(caf::detail::singletons::get_node_id())
       << endl;
  message_builder{argv + 1, argv + argc}.apply({
    on("-c", spro<uint16_t>, spro<uint16_t>, spro<uint16_t>)
    >> [](uint16_t p1, uint16_t p2, uint16_t gport) {
      CAF_LOGF_INFO("run in client mode");
      scoped_actor self;
      auto serv = io::remote_actor("localhost", p1);
      auto serv2 = io::remote_actor("localhost", p2);
      // remote_actor is supposed to return the same server
      // when connecting to the same host again
      {
        CAF_CHECK(serv == io::remote_actor("localhost", p1));
        CAF_CHECK(serv2 == io::remote_actor("127.0.0.1", p2));
      }
      // connect to published groups
      io::remote_group("whatever", "127.0.0.1", gport);
      auto c = self->spawn<client, monitored>(serv);
      self->receive(
        [&](const down_msg& dm) {
          CAF_CHECK_EQUAL(dm.source, c);
          CAF_CHECK_EQUAL(dm.reason, exit_reason::normal);
        }
      );
    },
    on("-s") >> [&] {
      CAF_PRINT("don't run remote actor (server mode)");
      test_remote_actor(argv[0], false);
    },
    on() >> [&] {
      test_remote_actor(argv[0], true);
    },
    others() >> [&] {
      CAF_PRINTERR("usage: " << argv[0] << " [-s PORT|-c PORT1 PORT2 GROUP_PORT]");
    }
  });
  await_all_actors_done();
  shutdown();
  // we either spawn a server or a client, in both cases
  // there must have been exactly one dtor called
  CAF_CHECK_EQUAL(s_destructors_called.load(), 1);
  return CAF_TEST_RESULT();
}
