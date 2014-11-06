#include <stack>
#include <chrono>
#include <iostream>
#include <functional>

#include "test.hpp"
#include "ping_pong.hpp"

#include "caf/all.hpp"

using namespace std;
using namespace caf;

namespace {

class event_testee : public sb_actor<event_testee> {

  friend class sb_actor<event_testee>;

  behavior wait4string;
  behavior wait4float;
  behavior wait4int;

  behavior& init_state = wait4int;

 public:

  event_testee() {
    wait4string = (
      on<string>() >> [=] {
        become(wait4int);
      },
      on<atom("get_state")>() >> [=] {
        return "wait4string";
      }
    );
    wait4float = (
      on<float>() >> [=] {
        become(wait4string);
      },
      on<atom("get_state")>() >> [=] {
        return "wait4float";
      }
    );
    wait4int = (
      on<int>() >> [=] {
        become(wait4float);
      },
      on<atom("get_state")>() >> [=] {
        return "wait4int";
      }
    );
  }

};

// quits after 5 timeouts
actor spawn_event_testee2(actor parent) {
  struct impl : event_based_actor {
    actor parent;
    impl(actor parent_actor) : parent(parent_actor) { }
    behavior wait4timeout(int remaining) {
      CAF_LOG_TRACE(CAF_ARG(remaining));
      return {
        after(chrono::milliseconds(1)) >> [=] {
          CAF_PRINT(CAF_ARG(remaining));
          if (remaining == 1) {
            send(parent, atom("t2done"));
            quit();
          }
          else become(wait4timeout(remaining - 1));
        }
      };
    }
    behavior make_behavior() override {
      return wait4timeout(5);
    }
  };
  return spawn<impl>(parent);
}

struct chopstick : public sb_actor<chopstick> {

  behavior taken_by(actor whom) {
    return (
      on<atom("take")>() >> [=] {
        return atom("busy");
      },
      on(atom("put"), whom) >> [=] {
        become(available);
      },
      on(atom("break")) >> [=] {
        quit();
      }
    );
  }

  behavior available;

  behavior& init_state = available;

  chopstick() {
    available = (
      on(atom("take"), arg_match) >> [=](actor whom) -> atom_value {
        become(taken_by(whom));
        return atom("taken");
      },
      on(atom("break")) >> [=] {
        quit();
      }
    );
  }

};

class testee_actor {

  void wait4string(blocking_actor* self) {
    bool string_received = false;
    self->do_receive (
      on<string>() >> [&] {
        string_received = true;
      },
      on<atom("get_state")>() >> [&] {
        return "wait4string";
      }
    )
    .until([&] { return string_received; });
  }

  void wait4float(blocking_actor* self) {
    bool float_received = false;
    self->do_receive (
      on<float>() >> [&] {
        float_received = true;
      },
      on<atom("get_state")>() >> [&] {
        return "wait4float";
      }
    )
    .until([&] { return float_received; });
    wait4string(self);
  }

 public:

  void operator()(blocking_actor* self) {
    self->receive_loop (
      on<int>() >> [&] {
        wait4float(self);
      },
      on<atom("get_state")>() >> [&] {
        return "wait4int";
      }
    );
  }

};

// self->receives one timeout and quits
void testee1(event_based_actor* self) {
  CAF_LOGF_TRACE("");
  self->become(after(chrono::milliseconds(10)) >> [=] {
    CAF_LOGF_TRACE("");
    self->unbecome();
  });
}

template <class Testee>
string behavior_test(scoped_actor& self, actor et) {
  string testee_name = detail::to_uniform_name(typeid(Testee));
  CAF_LOGF_TRACE(CAF_TARG(et, to_string) << ", " << CAF_ARG(testee_name));
  string result;
  self->send(et, 1);
  self->send(et, 2);
  self->send(et, 3);
  self->send(et, .1f);
  self->send(et, "hello " + testee_name);
  self->send(et, .2f);
  self->send(et, .3f);
  self->send(et, "hello again " + testee_name);
  self->send(et, "goodbye " + testee_name);
  self->send(et, atom("get_state"));
  self->receive (
    [&](const string& str) {
      result = str;
    },
    after(chrono::minutes(1)) >> [&]() {
      CAF_LOGF_ERROR(testee_name << " does not reply");
      throw runtime_error(testee_name + " does not reply");
    }
  );
  self->send_exit(et, exit_reason::user_shutdown);
  self->await_all_other_actors_done();
  return result;
}

behavior echo_actor(event_based_actor* self) {
  return (
    others() >> [=]() -> message {
      self->quit(exit_reason::normal);
      return self->last_dequeued();
    }
  );
}

behavior simple_mirror(event_based_actor* self) {
  return {
    others() >> [=] {
      return self->last_dequeued();
    }
  };
}

behavior high_priority_testee(event_based_actor* self) {
  self->send(self, atom("b"));
  self->send(message_priority::high, self, atom("a"));
  // 'a' must be self->received before 'b'
  return (
    on(atom("b")) >> [=] {
      CAF_FAILURE("received 'b' before 'a'");
      self->quit();
    },
    on(atom("a")) >> [=] {
      CAF_CHECKPOINT();
      self->become (
        on(atom("b")) >> [=] {
          CAF_CHECKPOINT();
          self->quit();
        },
        others() >> CAF_UNEXPECTED_MSG_CB(self)
      );
    },
    others() >> CAF_UNEXPECTED_MSG_CB(self)
  );
}

struct high_priority_testee_class : event_based_actor {
  behavior make_behavior() override {
    return high_priority_testee(this);
  }
};

struct master : event_based_actor {
  behavior make_behavior() override {
    return (
      on(atom("done")) >> [=] {
        CAF_PRINT("master: received done");
        quit(exit_reason::user_shutdown);
      }
    );
  }
};

struct slave : event_based_actor {

  slave(actor master_actor) : master{master_actor} { }

  behavior make_behavior() override {
    link_to(master);
    trap_exit(true);
    return (
      [=](const exit_msg& msg) {
        CAF_PRINT("slave: received exit message");
        quit(msg.reason);
      },
      others() >> CAF_UNEXPECTED_MSG_CB(this)
    );
  }

  actor master;

};

void test_spawn() {
  scoped_actor self;
  // check whether detached actors and scheduled actors interact w/o errors
  auto m = spawn<master, detached>();
  spawn<slave>(m);
  spawn<slave>(m);
  self->send(m, atom("done"));
  self->await_all_other_actors_done();
  CAF_CHECKPOINT();

  CAF_PRINT("test self->send()");
  self->send(self, 1, 2, 3, true);
  self->receive(on(1, 2, 3, true) >> [] { });
  self->send_tuple(self, message{});
  self->receive(on() >> [] { });
  self->await_all_other_actors_done();
  CAF_CHECKPOINT();

  CAF_PRINT("test self->receive with zero timeout");
  self->receive (
    others() >> CAF_UNEXPECTED_MSG_CB_REF(self),
    after(chrono::seconds(0)) >> [] { /* mailbox empty */ }
  );
  self->await_all_other_actors_done();
  CAF_CHECKPOINT();

  CAF_PRINT("test mirror"); {
    auto mirror = self->spawn<monitored>(simple_mirror);
    self->send(mirror, "hello mirror");
    self->receive (
      on("hello mirror") >> CAF_CHECKPOINT_CB(),
      others() >> CAF_UNEXPECTED_MSG_CB_REF(self)
    );
    self->send_exit(mirror, exit_reason::user_shutdown);
    self->receive (
      [&](const down_msg& dm) {
        if (dm.reason == exit_reason::user_shutdown) {
          CAF_CHECKPOINT();
        }
        else { CAF_UNEXPECTED_MSG_CB_REF(self); }
      },
      others() >> CAF_UNEXPECTED_MSG_CB_REF(self)
    );
    self->await_all_other_actors_done();
    CAF_CHECKPOINT();
  }

  CAF_PRINT("test detached mirror"); {
    auto mirror = self->spawn<monitored+detached>(simple_mirror);
    self->send(mirror, "hello mirror");
    self->receive (
      on("hello mirror") >> CAF_CHECKPOINT_CB(),
      others() >> CAF_UNEXPECTED_MSG_CB_REF(self)
    );
    self->send_exit(mirror, exit_reason::user_shutdown);
    self->receive (
      [&](const down_msg& dm) {
        if (dm.reason == exit_reason::user_shutdown) {
          CAF_CHECKPOINT();
        }
        else { CAF_UNEXPECTED_MSG(self); }
      },
      others() >> CAF_UNEXPECTED_MSG_CB_REF(self)
    );
    self->await_all_other_actors_done();
    CAF_CHECKPOINT();
  }

  CAF_PRINT("test priority aware mirror"); {
    auto mirror = self->spawn<monitored+priority_aware>(simple_mirror);
    CAF_CHECKPOINT();
    self->send(mirror, "hello mirror");
    self->receive (
      on("hello mirror") >> CAF_CHECKPOINT_CB(),
      others() >> CAF_UNEXPECTED_MSG_CB_REF(self)
    );
    self->send_exit(mirror, exit_reason::user_shutdown);
    self->receive (
      [&](const down_msg& dm) {
        if (dm.reason == exit_reason::user_shutdown) {
          CAF_CHECKPOINT();
        }
        else { CAF_UNEXPECTED_MSG(self); }
      },
      others() >> CAF_UNEXPECTED_MSG_CB_REF(self)
    );
    self->await_all_other_actors_done();
    CAF_CHECKPOINT();
  }

  CAF_PRINT("test echo actor");
  auto mecho = spawn(echo_actor);
  self->send(mecho, "hello echo");
  self->receive (
    on("hello echo") >> [] { },
    others() >> CAF_UNEXPECTED_MSG_CB_REF(self)
  );
  self->await_all_other_actors_done();
  CAF_CHECKPOINT();

  CAF_PRINT("test delayed_send()");
  self->delayed_send(self, chrono::seconds(1), 1, 2, 3);
  self->receive(on(1, 2, 3) >> [] { });
  self->await_all_other_actors_done();
  CAF_CHECKPOINT();

  CAF_PRINT("test timeout");
  self->receive(after(chrono::seconds(1)) >> [] { });
  CAF_CHECKPOINT();

  spawn(testee1);
  self->await_all_other_actors_done();
  CAF_CHECKPOINT();

  spawn_event_testee2(self);
  self->receive(on(atom("t2done")) >> CAF_CHECKPOINT_CB());
  self->await_all_other_actors_done();
  CAF_CHECKPOINT();

  auto cstk = spawn<chopstick>();
  self->send(cstk, atom("take"), self);
  self->receive (
    on(atom("taken")) >> [&] {
      self->send(cstk, atom("put"), self);
      self->send(cstk, atom("break"));
    },
    others() >> CAF_UNEXPECTED_MSG_CB_REF(self)
  );
  self->await_all_other_actors_done();
  CAF_CHECKPOINT();
  CAF_PRINT("test sync send");
  CAF_CHECKPOINT();
  auto sync_testee = spawn<blocking_api>([](blocking_actor* s) {
    s->receive (
      on("hi", arg_match) >> [&](actor from) {
        s->sync_send(from, "whassup?", s).await(
          on_arg_match >> [&](const string& str) -> string {
            CAF_CHECK(s->last_sender() != nullptr);
            CAF_CHECK_EQUAL(str, "nothing");
            return "goodbye!";
          },
          after(chrono::minutes(1)) >> [] {
            cerr << "PANIC!!!!" << endl;
            abort();
          }
        );
      },
      others() >> CAF_UNEXPECTED_MSG_CB_REF(s)
    );
  });
  self->monitor(sync_testee);
  self->send(sync_testee, "hi", self);
  self->receive (
    on("whassup?", arg_match) >> [&](actor other) -> std::string {
      CAF_CHECKPOINT();
      // this is NOT a reply, it's just an asynchronous message
      self->send(other, "a lot!");
      return "nothing";
    }
  );
  self->receive (
    on("goodbye!") >> CAF_CHECKPOINT_CB(),
    after(std::chrono::seconds(5)) >> CAF_UNEXPECTED_TOUT_CB()
  );
  self->receive (
    [&](const down_msg& dm) {
      CAF_CHECK_EQUAL(dm.reason, exit_reason::normal);
      CAF_CHECK_EQUAL(dm.source, sync_testee);
    }
  );
  self->await_all_other_actors_done();
  CAF_CHECKPOINT();

  self->sync_send(sync_testee, "!?").await(
    on<sync_exited_msg>() >> CAF_CHECKPOINT_CB(),
    others() >> CAF_UNEXPECTED_MSG_CB_REF(self),
    after(chrono::milliseconds(5)) >> CAF_UNEXPECTED_TOUT_CB()
  );

  CAF_CHECKPOINT();

  auto inflater = [](event_based_actor* s, const string& name, actor buddy) {
    CAF_LOGF_TRACE(CAF_ARG(s) << ", " << CAF_ARG(name)
            << ", " << CAF_TARG(buddy, to_string));
    s->become(
      [=](int n, const string& str) {
        s->send(buddy, n * 2, str + " from " + name);
      },
      on(atom("done")) >> [=] {
        s->quit();
      }
    );
  };
  auto joe = spawn(inflater, "Joe", self);
  auto bob = spawn(inflater, "Bob", joe);
  self->send(bob, 1, "hello actor");
  self->receive (
    on(4, "hello actor from Bob from Joe") >> CAF_CHECKPOINT_CB(),
    others() >> CAF_UNEXPECTED_MSG_CB_REF(self)
  );
  // kill joe and bob
  auto poison_pill = make_message(atom("done"));
  anon_send_tuple(joe, poison_pill);
  anon_send_tuple(bob, poison_pill);
  self->await_all_other_actors_done();

  function<actor (const string&, const actor&)> spawn_next;
  // it's safe to capture spawn_next as reference here, because
  // - it is guaranteeed to outlive kr34t0r by general scoping rules
  // - the lambda is always executed in the current actor's thread
  // but using spawn_next in a message handler could
  // still cause undefined behavior!
  auto kr34t0r = [&spawn_next](event_based_actor* s, const string& name, actor pal) {
    if (name == "Joe" && !pal) {
      pal = spawn_next("Bob", s);
    }
    s->become (
      others() >> [=] {
        // forward message and die
        s->send_tuple(pal, s->last_dequeued());
        s->quit();
      }
    );
  };
  spawn_next = [&kr34t0r](const string& name, const actor& pal) {
    return spawn(kr34t0r, name, pal);
  };
  auto joe_the_second = spawn(kr34t0r, "Joe", invalid_actor);
  self->send(joe_the_second, atom("done"));
  self->await_all_other_actors_done();

  auto f = [](const string& name) -> behavior {
    return (
      on(atom("get_name")) >> [name] {
        return make_cow_tuple(atom("name"), name);
      }
    );
  };
  auto a1 = spawn(f, "alice");
  auto a2 = spawn(f, "bob");
  self->send(a1, atom("get_name"));
  self->receive (
    on(atom("name"), arg_match) >> [&](const string& name) {
      CAF_CHECK_EQUAL(name, "alice");
    }
  );
  self->send(a2, atom("get_name"));
  self->receive (
    on(atom("name"), arg_match) >> [&](const string& name) {
      CAF_CHECK_EQUAL(name, "bob");
    }
  );
  self->send_exit(a1, exit_reason::user_shutdown);
  self->send_exit(a2, exit_reason::user_shutdown);
  self->await_all_other_actors_done();
  CAF_CHECKPOINT();

  auto res1 = behavior_test<testee_actor>(self, spawn<blocking_api>(testee_actor{}));
  CAF_CHECK_EQUAL("wait4int", res1);
  CAF_CHECK_EQUAL(behavior_test<event_testee>(self, spawn<event_testee>()), "wait4int");
  self->await_all_other_actors_done();
  CAF_CHECKPOINT();

  // create some actors linked to one single actor
  // and kill them all through killing the link
  auto legion = spawn([](event_based_actor* s) {
    CAF_PRINT("spawn 100 actors");
    for (int i = 0; i < 100; ++i) {
      s->spawn<event_testee, linked>();
    }
    s->become(others() >> CAF_UNEXPECTED_MSG_CB(s));
  });
  self->send_exit(legion, exit_reason::user_shutdown);
  self->await_all_other_actors_done();
  CAF_CHECKPOINT();
  self->trap_exit(true);
  auto ping_actor = self->spawn<monitored+blocking_api>(ping, 10);
  auto pong_actor = self->spawn<monitored+blocking_api>(pong, ping_actor);
  self->link_to(pong_actor);
  int i = 0;
  int flags = 0;
  self->delayed_send(self, chrono::seconds(1), atom("FooBar"));
  // wait for DOWN and EXIT messages of pong
  self->receive_for(i, 4) (
    [&](const exit_msg& em) {
      CAF_CHECK_EQUAL(em.source, pong_actor);
      CAF_CHECK_EQUAL(em.reason, exit_reason::user_shutdown);
      flags |= 0x01;
    },
    [&](const down_msg& dm) {
      if (dm.source == pong_actor) {
        flags |= 0x02;
        CAF_CHECK_EQUAL(dm.reason, exit_reason::user_shutdown);
      }
      else if (dm.source == ping_actor) {
        flags |= 0x04;
        CAF_CHECK_EQUAL(dm.reason, exit_reason::normal);
      }
    },
    [&](const atom_value& val) {
      CAF_CHECK(val == atom("FooBar"));
      flags |= 0x08;
    },
    others() >> [&]() {
      CAF_FAILURE("unexpected message: " << to_string(self->last_dequeued()));
    },
    after(chrono::seconds(5)) >> [&]() {
      CAF_FAILURE("timeout in file " << __FILE__ << " in line " << __LINE__);
    }
  );
  // wait for termination of all spawned actors
  self->await_all_other_actors_done();
  CAF_CHECK_EQUAL(flags, 0x0F);
  // verify pong messages
  CAF_CHECK_EQUAL(pongs(), 10);
  CAF_CHECKPOINT();
  spawn<priority_aware>(high_priority_testee);
  self->await_all_other_actors_done();
  CAF_CHECKPOINT();
  spawn<high_priority_testee_class, priority_aware>();
  self->await_all_other_actors_done();
  // test sending message to self via scoped_actor
  self->send(self, atom("check"));
  self->receive (
    on(atom("check")) >> [] {
      CAF_CHECKPOINT();
    }
  );
  CAF_CHECKPOINT();
  CAF_PRINT("check whether timeouts trigger more than once");
  auto counter = make_shared<int>(0);
  auto sleeper = self->spawn<monitored>([=](event_based_actor* s) {
    return after(std::chrono::milliseconds(1)) >> [=] {
      CAF_PRINT("received timeout #" << (*counter + 1));
      if (++*counter > 3) {
        CAF_CHECKPOINT();
        s->quit();
      }
    };
  });
  self->receive(
    [&](const down_msg& msg) {
      CAF_CHECK_EQUAL(msg.source, sleeper);
      CAF_CHECK_EQUAL(msg.reason, exit_reason::normal);
    }
  );
  CAF_CHECKPOINT();
}

void counting_actor(event_based_actor* self) {
  for (int i = 0; i < 100; ++i) {
    self->send(self, atom("dummy"));
  }
  CAF_CHECK_EQUAL(self->mailbox().count(), 100);
  for (int i = 0; i < 100; ++i) {
    self->send(self, atom("dummy"));
  }
  CAF_CHECK_EQUAL(self->mailbox().count(), 200);
}

} // namespace <anonymous>

int main() {
  CAF_TEST(test_spawn);
  spawn(counting_actor);
  await_all_actors_done();
  CAF_CHECKPOINT();
  test_spawn();
  CAF_CHECKPOINT();
  await_all_actors_done();
  CAF_CHECKPOINT();
  // test setting exit reasons for scoped actors
  { // lifetime scope of self
    scoped_actor self;
    self->spawn<linked>([]() -> behavior { return others() >> [] {}; });
    self->planned_exit_reason(exit_reason::user_defined);
  }
  await_all_actors_done();
  CAF_CHECKPOINT();
  shutdown();
  CAF_CHECKPOINT();
  return CAF_TEST_RESULT();
}
