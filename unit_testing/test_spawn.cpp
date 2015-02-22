#include <stack>
#include <atomic>
#include <chrono>
#include <iostream>
#include <functional>

#include "test.hpp"
#include "ping_pong.hpp"

#include "caf/all.hpp"

using namespace std;
using namespace caf;

namespace {

std::atomic<long> s_max_actor_instances;
std::atomic<long> s_actor_instances;

void inc_actor_instances() {
  long v1 = ++s_actor_instances;
  long v2 = s_max_actor_instances.load();
  while (v1 > v2) {
    s_max_actor_instances.compare_exchange_weak(v2, v1);
  }
}

void dec_actor_instances() {
  --s_actor_instances;
}

class event_testee : public sb_actor<event_testee> {
 public:
  event_testee();
  ~event_testee();

  behavior wait4string;
  behavior wait4float;
  behavior wait4int;

  behavior& init_state = wait4int;
};

event_testee::event_testee() {
  inc_actor_instances();
  wait4string.assign(
    [=](const std::string&) {
      become(wait4int);
    },
    [=](get_atom) {
      return "wait4string";
    }
  );
  wait4float.assign(
    [=](float) {
      become(wait4string);
    },
    [=](get_atom) {
      return "wait4float";
    }
  );
  wait4int.assign(
    [=](int) {
      become(wait4float);
    },
    [=](get_atom) {
      return "wait4int";
    }
  );
}

event_testee::~event_testee() {
  dec_actor_instances();
}

// quits after 5 timeouts
actor spawn_event_testee2(actor parent) {
  struct impl : event_based_actor {
    actor parent;
    impl(actor parent_actor) : parent(parent_actor) {
      inc_actor_instances();
    }
    ~impl() {
      dec_actor_instances();
    }
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
    return {
      on<atom("take")>() >> [=] {
        return atom("busy");
      },
      on(atom("put"), whom) >> [=] {
        become(available);
      },
      on(atom("break")) >> [=] {
        quit();
      }
    };
  }

  behavior available;

  behavior& init_state = available;

  chopstick();
  ~chopstick();
};

chopstick::chopstick() {
  inc_actor_instances();
  available.assign(
    on(atom("take"), arg_match) >> [=](actor whom) -> atom_value {
      become(taken_by(whom));
      return atom("taken");
    },
    on(atom("break")) >> [=] {
      quit();
    }
  );
}

chopstick::~chopstick() {
  dec_actor_instances();
}

class testee_actor : public blocking_actor {
 public:
  testee_actor();
  ~testee_actor();
  void act() override;

 private:
  void wait4string();
  void wait4float();
};

testee_actor::testee_actor() {
  inc_actor_instances();
}

testee_actor::~testee_actor() {
  dec_actor_instances();
}

void testee_actor::act() {
  receive_loop (
    [&](int) {
      wait4float();
    },
    [&](get_atom) {
      return "wait4int";
    }
  );
}

void testee_actor::wait4string() {
  bool string_received = false;
  do_receive (
    [&](const string&) {
      string_received = true;
    },
    [&](get_atom) {
      return "wait4string";
    }
  )
  .until([&] { return string_received; });
}

void testee_actor::wait4float() {
  bool float_received = false;
  do_receive (
    [&](float) {
      float_received = true;
    },
    [&](get_atom) {
      return "wait4float";
    }
  )
  .until([&] { return float_received; });
  wait4string();
}

// self->receives one timeout and quits
class testee1 : public event_based_actor {
 public:
  testee1();
  ~testee1();
  behavior make_behavior() override;
};

testee1::testee1() {
  inc_actor_instances();
}

testee1::~testee1() {
  dec_actor_instances();
}

behavior testee1::make_behavior() {
  CAF_LOGF_TRACE("");
  return {
    after(chrono::milliseconds(10)) >> [=] {
      CAF_LOGF_TRACE("");
      unbecome();
    }
  };
}

string behavior_test(scoped_actor& self, actor et) {
  CAF_LOGF_TRACE(CAF_TARG(et, to_string));
  string result;
  self->send(et, 1);
  self->send(et, 2);
  self->send(et, 3);
  self->send(et, .1f);
  self->send(et, "hello");
  self->send(et, .2f);
  self->send(et, .3f);
  self->send(et, "hello again");
  self->send(et, "goodbye");
  self->send(et, get_atom::value);
  self->receive (
    [&](const string& str) {
      result = str;
    },
    after(chrono::minutes(1)) >> [&]() {
      CAF_LOGF_ERROR("actor does not reply");
      throw runtime_error("actor does not reply");
    }
  );
  self->send_exit(et, exit_reason::user_shutdown);
  self->await_all_other_actors_done();
  return result;
}

class echo_actor : public event_based_actor {
 public:
  echo_actor();
  ~echo_actor();
  behavior make_behavior() override;
};

echo_actor::echo_actor() {
  inc_actor_instances();
}

echo_actor::~echo_actor() {
  dec_actor_instances();
}

behavior echo_actor::make_behavior() {
  return {
    others >> [=]() -> message {
      quit(exit_reason::normal);
      return current_message();
    }
  };
}

class simple_mirror : public event_based_actor {
 public:
  simple_mirror();
  ~simple_mirror();
  behavior make_behavior() override;
};

simple_mirror::simple_mirror() {
  inc_actor_instances();
}

simple_mirror::~simple_mirror() {
  dec_actor_instances();
}

behavior simple_mirror::make_behavior() {
  return {
    others >> [=] {
      CAF_CHECKPOINT();
      return current_message();
    }
  };
}

behavior high_priority_testee(event_based_actor* self) {
  self->send(self, atom("b"));
  self->send(message_priority::high, self, atom("a"));
  // 'a' must be self->received before 'b'
  return {
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
        others >> CAF_UNEXPECTED_MSG_CB(self)
      );
    },
    others >> CAF_UNEXPECTED_MSG_CB(self)
  };
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
    return {
      [=](const exit_msg& msg) {
        CAF_PRINT("slave: received exit message");
        quit(msg.reason);
      },
      others >> CAF_UNEXPECTED_MSG_CB(this)
    };
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
  self->send(self, message{});
  self->receive(on() >> [] { });
  self->await_all_other_actors_done();
  CAF_CHECKPOINT();

  CAF_PRINT("test self->receive with zero timeout");
  self->receive (
    others >> CAF_UNEXPECTED_MSG_CB_REF(self),
    after(chrono::seconds(0)) >> [] { /* mailbox empty */ }
  );
  self->await_all_other_actors_done();
  CAF_CHECKPOINT();

  CAF_PRINT("test mirror"); {
    auto mirror = self->spawn<simple_mirror, monitored>();
    self->send(mirror, "hello mirror");
    self->receive (
      on("hello mirror") >> CAF_CHECKPOINT_CB(),
      others >> CAF_UNEXPECTED_MSG_CB_REF(self)
    );
    self->send_exit(mirror, exit_reason::user_shutdown);
    self->receive (
      [&](const down_msg& dm) {
        if (dm.reason == exit_reason::user_shutdown) {
          CAF_CHECKPOINT();
        }
        else { CAF_UNEXPECTED_MSG_CB_REF(self); }
      },
      others >> CAF_UNEXPECTED_MSG_CB_REF(self)
    );
    self->await_all_other_actors_done();
    CAF_CHECKPOINT();
  }

  CAF_PRINT("test detached mirror"); {
    auto mirror = self->spawn<simple_mirror, monitored+detached>();
    self->send(mirror, "hello mirror");
    self->receive (
      on("hello mirror") >> CAF_CHECKPOINT_CB(),
      others >> CAF_UNEXPECTED_MSG_CB_REF(self)
    );
    self->send_exit(mirror, exit_reason::user_shutdown);
    self->receive (
      [&](const down_msg& dm) {
        if (dm.reason == exit_reason::user_shutdown) {
          CAF_CHECKPOINT();
        }
        else { CAF_UNEXPECTED_MSG(self); }
      },
      others >> CAF_UNEXPECTED_MSG_CB_REF(self)
    );
    self->await_all_other_actors_done();
    CAF_CHECKPOINT();
  }

  CAF_PRINT("test priority aware mirror"); {
    auto mirror = self->spawn<simple_mirror, monitored + priority_aware>();
    CAF_CHECKPOINT();
    self->send(mirror, "hello mirror");
    self->receive (
      on("hello mirror") >> CAF_CHECKPOINT_CB(),
      others >> CAF_UNEXPECTED_MSG_CB_REF(self)
    );
    self->send_exit(mirror, exit_reason::user_shutdown);
    self->receive (
      [&](const down_msg& dm) {
        if (dm.reason == exit_reason::user_shutdown) {
          CAF_CHECKPOINT();
        }
        else { CAF_UNEXPECTED_MSG(self); }
      },
      others >> CAF_UNEXPECTED_MSG_CB_REF(self)
    );
    self->await_all_other_actors_done();
    CAF_CHECKPOINT();
  }

  CAF_PRINT("test echo actor");
  auto mecho = spawn<echo_actor>();
  self->send(mecho, "hello echo");
  self->receive (
    on("hello echo") >> [] { },
    others >> CAF_UNEXPECTED_MSG_CB_REF(self)
  );
  self->await_all_other_actors_done();
  CAF_CHECKPOINT();

  CAF_PRINT("test delayed_send()");
  self->delayed_send(self, chrono::milliseconds(1), 1, 2, 3);
  self->receive(on(1, 2, 3) >> [] { });
  self->await_all_other_actors_done();
  CAF_CHECKPOINT();

  CAF_PRINT("test timeout");
  self->receive(after(chrono::milliseconds(1)) >> [] { });
  CAF_CHECKPOINT();

  spawn<testee1>();
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
    others >> CAF_UNEXPECTED_MSG_CB_REF(self)
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
            CAF_CHECK(s->current_sender() != nullptr);
            CAF_CHECK_EQUAL(str, "nothing");
            return "goodbye!";
          },
          after(chrono::minutes(1)) >> [] {
            cerr << "PANIC!!!!" << endl;
            abort();
          }
        );
      },
      others >> CAF_UNEXPECTED_MSG_CB_REF(s)
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
    after(std::chrono::seconds(1)) >> CAF_UNEXPECTED_TOUT_CB()
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
    others >> CAF_UNEXPECTED_MSG_CB_REF(self),
    after(chrono::milliseconds(1)) >> CAF_UNEXPECTED_TOUT_CB()
  );

  CAF_CHECKPOINT();
  struct inflater : public event_based_actor {
   public:
    inflater(string name, actor buddy)
        : m_name(std::move(name)),
          m_buddy(std::move(buddy)) {
      inc_actor_instances();
    }
    ~inflater() {
      dec_actor_instances();
    }
    behavior make_behavior() override {
      return {
        [=](int n, const string& str) {
          send(m_buddy, n * 2, str + " from " + m_name);
        },
        on(atom("done")) >> [=] {
          quit();
        }
      };
    }

   private:
    string m_name;
    actor m_buddy;
  };
  auto joe = spawn<inflater>("Joe", self);
  auto bob = spawn<inflater>("Bob", joe);
  self->send(bob, 1, "hello actor");
  self->receive (
    on(4, "hello actor from Bob from Joe") >> CAF_CHECKPOINT_CB(),
    others >> CAF_UNEXPECTED_MSG_CB_REF(self)
  );
  // kill joe and bob
  auto poison_pill = make_message(atom("done"));
  anon_send(joe, poison_pill);
  anon_send(bob, poison_pill);
  self->await_all_other_actors_done();

  class kr34t0r : public event_based_actor {
   public:
    kr34t0r(string name, actor pal)
        : m_name(std::move(name)),
          m_pal(std::move(pal)) {
      inc_actor_instances();
    }
    ~kr34t0r() {
      dec_actor_instances();
    }
    behavior make_behavior() override {
      if (m_name == "Joe" && m_pal == invalid_actor) {
        m_pal = spawn<kr34t0r>("Bob", this);
      }
      return {
        others >> [=] {
          // forward message and die
          send(m_pal, current_message());
          quit();
        }
      };
    }
    void on_exit() {
      m_pal = invalid_actor; // break cycle
    }

   private:
    string m_name;
    actor m_pal;
  };
  auto joe_the_second = spawn<kr34t0r>("Joe", invalid_actor);
  self->send(joe_the_second, atom("done"));
  self->await_all_other_actors_done();

  auto f = [](const string& name) -> behavior {
    return (
      on(atom("get_name")) >> [name] {
        return make_message(atom("name"), name);
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

  auto res1 = behavior_test(self, spawn<testee_actor, blocking_api>());
  CAF_CHECK_EQUAL("wait4int", res1);
  CAF_CHECK_EQUAL(behavior_test(self, spawn<event_testee>()), "wait4int");
  self->await_all_other_actors_done();
  CAF_CHECKPOINT();

  // create some actors linked to one single actor
  // and kill them all through killing the link
  class legion_actor : public event_based_actor {
   public:
    legion_actor() {
      inc_actor_instances();
      for (int i = 0; i < 100; ++i) {
        spawn<event_testee, linked>();
      }
    }
    ~legion_actor() {
      dec_actor_instances();
    }
    behavior make_behavior() override {
      return {
        others >> CAF_UNEXPECTED_MSG_CB(this)
      };
    }
  };
  auto legion = spawn<legion_actor>();
  self->send_exit(legion, exit_reason::user_shutdown);
  self->await_all_other_actors_done();
  CAF_CHECKPOINT();
  self->trap_exit(true);
  auto ping_actor = self->spawn<monitored+blocking_api>(ping, 10);
  auto pong_actor = self->spawn<monitored+blocking_api>(pong, ping_actor);
  self->link_to(pong_actor);
  int i = 0;
  int flags = 0;
  self->delayed_send(self, chrono::milliseconds(10), atom("FooBar"));
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
    others >> [&]() {
      CAF_FAILURE("unexpected message: " << to_string(self->current_message()));
    },
    after(chrono::milliseconds(500)) >> [&]() {
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

class counting_actor : public event_based_actor {
 public:
  counting_actor();
  ~counting_actor();
  behavior make_behavior() override;
};

counting_actor::counting_actor() {
  inc_actor_instances();
}

counting_actor::~counting_actor() {
  dec_actor_instances();
}

behavior counting_actor::make_behavior() {
  for (int i = 0; i < 100; ++i) {
    send(this, atom("dummy"));
  }
  CAF_CHECK_EQUAL(mailbox().count(), 100);
  for (int i = 0; i < 100; ++i) {
    send(this, atom("dummy"));
  }
  CAF_CHECK_EQUAL(mailbox().count(), 200);
  return {};
}

// tests attach_functor() inside of an actor's constructor
void test_constructor_attach() {
  class testee : public event_based_actor {
   public:
    testee(actor buddy) : m_buddy(buddy) {
      attach_functor([=](uint32_t reason) {
        send(m_buddy, atom("done"), reason);
      });
    }
    behavior make_behavior() {
      return {
        on(atom("die")) >> [=] {
          quit(exit_reason::user_shutdown);
        }
      };
    }
   private:
    actor m_buddy;
  };
  class spawner : public event_based_actor {
   public:
    spawner() : m_downs(0) {
    }
    behavior make_behavior() {
      m_testee = spawn<testee, monitored>(this);
      return {
        [=](const down_msg& msg) {
          CAF_CHECK_EQUAL(msg.reason, exit_reason::user_shutdown);
          if (++m_downs == 2) {
            quit(msg.reason);
          }
        },
        on(atom("done"), arg_match) >> [=](uint32_t reason) {
          CAF_CHECK_EQUAL(reason, exit_reason::user_shutdown);
          if (++m_downs == 2) {
            quit(reason);
          }
        },
        others >> [=] {
          forward_to(m_testee);
        }
      };
    }
   private:
    int m_downs;
    actor m_testee;
  };
  anon_send(spawn<spawner>(), atom("die"));
}

class exception_testee : public event_based_actor {
 public:
  exception_testee() {
    set_exception_handler([](const std::exception_ptr&) -> optional<uint32_t> {
      return exit_reason::user_defined + 2;
    });
  }
  behavior make_behavior() override {
    return {
      others >> [] {
        throw std::runtime_error("whatever");
      }
    };
  }
};

void test_custom_exception_handler() {
  auto handler = [](const std::exception_ptr& eptr) -> optional<uint32_t> {
    try {
      std::rethrow_exception(eptr);
    }
    catch (std::runtime_error&) {
      return exit_reason::user_defined;
    }
    catch (...) {
      // "fall through"
    }
    return exit_reason::user_defined + 1;
  };
  scoped_actor self;
  auto testee1 = self->spawn<monitored>([=](event_based_actor* eb_self) {
    eb_self->set_exception_handler(handler);
    throw std::runtime_error("ping");
  });
  auto testee2 = self->spawn<monitored>([=](event_based_actor* eb_self) {
    eb_self->set_exception_handler(handler);
    throw std::logic_error("pong");
  });
  auto testee3 = self->spawn<exception_testee, monitored>();
  self->send(testee3, "foo");
  // receive all down messages
  auto i = 0;
  self->receive_for(i, 3)(
    [&](const down_msg& dm) {
      if (dm.source == testee1) {
        CAF_CHECK_EQUAL(dm.reason, exit_reason::user_defined);
      }
      else if (dm.source == testee2) {
        CAF_CHECK_EQUAL(dm.reason, exit_reason::user_defined + 1);
      }
      else if (dm.source == testee3) {
        CAF_CHECK_EQUAL(dm.reason, exit_reason::user_defined + 2);
      }
      else {
        CAF_CHECK(false); // report error
      }
    }
  );
}

using abc_atom = atom_constant<atom("abc")>;

using typed_testee = typed_actor<replies_to<abc_atom>::with<std::string>>;

typed_testee::behavior_type testee() {
  return {
    [](abc_atom) {
      CAF_PRINT("received abc_atom");
      return "abc";
    }
  };
}

void test_typed_testee() {
  CAF_PRINT("test_typed_testee");
  scoped_actor self;
  auto x = spawn_typed(testee);
  self->sync_send(x, abc_atom()).await(
    [](const std::string& str) {
      CAF_CHECK_EQUAL(str, "abc");
    }
  );
  self->send_exit(x, exit_reason::user_shutdown);
}

} // namespace <anonymous>

int main() {
  CAF_TEST(test_spawn);
  { // lifetime scope of temporary counting_actor handle
    spawn<counting_actor>();
    await_all_actors_done();
  }
  CAF_CHECKPOINT();
  test_spawn();
  CAF_CHECKPOINT();
  await_all_actors_done();
  CAF_CHECKPOINT();
  test_typed_testee();
  CAF_CHECKPOINT();
  await_all_actors_done();
  CAF_CHECKPOINT();
  test_constructor_attach();
  CAF_CHECKPOINT();
  test_custom_exception_handler();
  CAF_CHECKPOINT();
  // test setting exit reasons for scoped actors
  { // lifetime scope of self
    scoped_actor self;
    self->spawn<linked>([]() -> behavior { return others >> [] {}; });
    self->planned_exit_reason(exit_reason::user_defined);
  }
  await_all_actors_done();
  CAF_CHECKPOINT();
  shutdown();
  CAF_CHECKPOINT();
  CAF_CHECK_EQUAL(s_actor_instances.load(), 0);
  CAF_PRINT("max. nr. of actor instances: " << s_max_actor_instances.load());
  return CAF_TEST_RESULT();
}
