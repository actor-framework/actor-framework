// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/actor_system.hpp"
#include "caf/blocking_actor.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/function_view.hpp"
#include "caf/init_global_meta_objects.hpp"
#include "caf/log/test.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/typed_event_based_actor.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <stack>

CAF_BEGIN_TYPE_ID_BLOCK(dynamic_spawn_test, caf::first_custom_type_id + 105)

  CAF_ADD_ATOM(dynamic_spawn_test, abc_atom)
  CAF_ADD_ATOM(dynamic_spawn_test, name_atom)

CAF_END_TYPE_ID_BLOCK(dynamic_spawn_test)

using namespace caf;

CAF_PUSH_DEPRECATED_WARNING

namespace {

std::atomic<long> s_max_actor_instances;
std::atomic<long> s_actor_instances;

void inc_actor_instances() {
  long v1 = ++s_actor_instances;
  long v2 = s_max_actor_instances.load();
  while (v1 > v2) {
    s_max_actor_instances.compare_exchange_strong(v2, v1);
  }
}

void dec_actor_instances() {
  --s_actor_instances;
}

class event_testee : public event_based_actor {
public:
  event_testee(actor_config& cfg) : event_based_actor(cfg) {
    inc_actor_instances();
    wait4string.assign([this](const std::string&) { become(wait4int); },
                       [](get_atom) { return "wait4string"; });
    wait4float.assign([this](float) { become(wait4string); },
                      [](get_atom) { return "wait4float"; });
    wait4int.assign([this](int) { become(wait4float); },
                    [](get_atom) { return "wait4int"; });
  }

  ~event_testee() override {
    dec_actor_instances();
  }

  behavior make_behavior() override {
    return wait4int;
  }

  behavior wait4string;
  behavior wait4float;
  behavior wait4int;
};

// quits after 5 timeouts
actor spawn_event_testee2(scoped_actor& parent) {
  struct wrapper : event_based_actor {
    actor parent;
    wrapper(actor_config& cfg, actor parent_actor)
      : event_based_actor(cfg), parent(std::move(parent_actor)) {
      inc_actor_instances();
    }
    ~wrapper() override {
      dec_actor_instances();
    }
    behavior wait4timeout(int remaining) {
      return {
        after(std::chrono::milliseconds(1)) >>
          [this, remaining] {
            log::test::debug("remaining: {}", std::to_string(remaining));
            if (remaining == 1) {
              mail(ok_atom_v).send(parent);
              quit();
            } else
              become(wait4timeout(remaining - 1));
          },
      };
    }
    behavior make_behavior() override {
      return wait4timeout(5);
    }
  };
  return parent->spawn<wrapper>(parent);
}

class testee_actor : public blocking_actor {
public:
  testee_actor(actor_config& cfg) : blocking_actor(cfg) {
    inc_actor_instances();
  }

  ~testee_actor() override {
    dec_actor_instances();
  }

  void act() override {
    bool running = true;
    receive_while(running)([&](int) { wait4float(); },
                           [&](get_atom) { return "wait4int"; },
                           [&](exit_msg& em) {
                             if (em.reason) {
                               fail_state(std::move(em.reason));
                               running = false;
                             }
                           });
  }

private:
  void wait4string() {
    bool string_received = false;
    do_receive([&](const std::string&) { string_received = true; },
               [&](get_atom) { return "wait4string"; })
      .until([&] { return string_received; });
  }

  void wait4float() {
    bool float_received = false;
    do_receive([&](float) { float_received = true; },
               [&](get_atom) { return "wait4float"; })
      .until([&] { return float_received; });
    wait4string();
  }
};

// self->receives one timeout and quits
class testee1 : public event_based_actor {
public:
  testee1(actor_config& cfg) : event_based_actor(cfg) {
    inc_actor_instances();
  }

  ~testee1() override {
    dec_actor_instances();
  }

  behavior make_behavior() override {
    return {
      after(std::chrono::milliseconds(10)) >> [this] { unbecome(); },
    };
  }
};

class echo_actor : public event_based_actor {
public:
  echo_actor(actor_config& cfg) : event_based_actor(cfg) {
    inc_actor_instances();
  }

  ~echo_actor() override {
    dec_actor_instances();
  }

  behavior make_behavior() override {
    set_default_handler(reflect);
    return {
      [] {
        // nop
      },
    };
  }
};

class simple_mirror : public event_based_actor {
public:
  simple_mirror(actor_config& cfg) : event_based_actor(cfg) {
    inc_actor_instances();
  }

  ~simple_mirror() override {
    dec_actor_instances();
  }

  behavior make_behavior() override {
    set_default_handler(reflect);
    return {
      [] {
        // nop
      },
    };
  }
};

behavior master(event_based_actor* self) {
  return {
    [self](ok_atom) {
      log::test::debug("master: received done");
      self->quit(exit_reason::user_shutdown);
    },
  };
}

behavior slave(event_based_actor* self, const actor& master) {
  self->link_to(master);
  self->set_exit_handler([self](exit_msg& msg) {
    log::test::debug("slave: received exit message");
    self->quit(msg.reason);
  });
  return {
    [] {
      // nop
    },
  };
}

class counting_actor : public event_based_actor {
public:
  counting_actor(actor_config& cfg) : event_based_actor(cfg) {
    inc_actor_instances();
  }

  ~counting_actor() override {
    dec_actor_instances();
  }

  behavior make_behavior() override {
    for (int i = 0; i < 100; ++i) {
      mail(ok_atom_v).send(this);
    }
    if (mailbox().size() != 100u)
      CAF_RAISE_ERROR("mailbox size is not 100");
    for (int i = 0; i < 100; ++i) {
      mail(ok_atom_v).send(this);
    }
    if (mailbox().size() != 200u)
      CAF_RAISE_ERROR("mailbox size is not 200");
    return {};
  }
};

struct fixture {
  actor_system_config cfg;
  // put inside a union to control ctor/dtor timing
  union {
    actor_system system;
  };

  fixture() {
    new (&system) actor_system(cfg);
  }

  ~fixture() {
    system.~actor_system();
    // destructor of actor_system must make sure all
    // destructors of all actors have been run
    if (s_actor_instances.load() != 0)
      CAF_RAISE_ERROR("destructor of all actor instances not run");
    log::test::debug("max. # of actor instances: {}",
                     s_max_actor_instances.load());
  }
};

WITH_FIXTURE(test::fixture::deterministic) {

TEST("mirror") {
  scoped_actor self{sys};
  auto mirror = self->spawn<simple_mirror>();
  auto dummy = self->spawn([this, mirror](event_based_actor* ptr) -> behavior {
    ptr->mail("hello mirror").send(mirror);
    return {[this](const std::string& msg) { check_eq(msg, "hello mirror"); }};
  });
  dispatch_messages();
  /*
  self->mail("hello mirror").send(mirror);
  run();
  self->receive (
    [](const std::string& msg) {
      check_eq(msg, "hello mirror");
    }
  );
  */
}

} // WITH_FIXTURE(test::fixture::deterministic)

WITH_FIXTURE(fixture) {

TEST("count mailbox") {
  system.spawn<counting_actor>();
}

TEST("detached actors and scheduled actors") {
  scoped_actor self{system};
  // check whether detached actors and scheduled actors interact w/o errors
  auto m = system.spawn<detached>(master);
  system.spawn(slave, m);
  system.spawn(slave, m);
  self->mail(ok_atom_v).send(m);
}

TEST("receive with zero timeout") {
  scoped_actor self{system};
  self->receive([&] { fail("Unexpected message"); },
                after(std::chrono::seconds(0)) >>
                  [] {
                    // mailbox empty
                  });
}

TEST("detached mirror") {
  scoped_actor self{system};
  auto mirror = self->spawn<simple_mirror, detached>();
  self->mail("hello mirror").send(mirror);
  self->receive(
    [this](const std::string& msg) { check_eq(msg, "hello mirror"); });
}

TEST("send to self") {
  scoped_actor self{system};
  self->mail(1, 2, 3, true).send(self);
  self->receive([this](int a, int b, int c, bool d) {
    check_eq(a, 1);
    check_eq(b, 2);
    check_eq(c, 3);
    check_eq(d, true);
  });
  self->mail(message{}).send(self);
  self->receive([] {});
}

TEST("echo actor messaging") {
  scoped_actor self{system};
  auto mecho = system.spawn<echo_actor>();
  self->mail("hello echo").send(mecho);
  self->receive(
    [this](const std::string& arg) { check_eq(arg, "hello echo"); });
}

TEST("delayed send") {
  scoped_actor self{system};
  self->mail(1, 2, 3).delay(std::chrono::milliseconds(1)).send(self);
  self->receive([this](int a, int b, int c) {
    check_eq(a, 1);
    check_eq(b, 2);
    check_eq(c, 3);
  });
}

TEST("delayed spawn") {
  scoped_actor self{system};
  self->receive(after(std::chrono::milliseconds(1)) >> [] {});
  system.spawn<testee1>();
}

TEST("spawn event testee2 test") {
  scoped_actor self{system};
  spawn_event_testee2(self);
  self->receive([](ok_atom) { log::test::debug("Received 'ok'"); });
}

TEST("function_spawn") {
  scoped_actor self{system};
  auto f = [](const std::string& name) -> behavior {
    return ([name](get_atom) { return make_result(name_atom_v, name); });
  };
  auto a1 = system.spawn(f, "alice");
  auto a2 = system.spawn(f, "bob");
  self->mail(get_atom_v).send(a1);
  self->receive(
    [&](name_atom, const std::string& name) { check_eq(name, "alice"); });
  self->mail(get_atom_v).send(a2);
  self->receive(
    [&](name_atom, const std::string& name) { check_eq(name, "bob"); });
  self->send_exit(a1, exit_reason::user_shutdown);
  self->send_exit(a2, exit_reason::user_shutdown);
}

using typed_testee = typed_actor<result<std::string>(abc_atom)>;

typed_testee::behavior_type testee() {
  return {[](abc_atom) {
    log::test::debug("received 'abc'");
    return "abc";
  }};
}

TEST("typed await") {
  scoped_actor self{system};
  auto f = make_function_view(system.spawn(testee));
  check_eq(f(abc_atom_v), "abc");
}

// tests attach_functor() inside of an actor's constructor
TEST("constructor attach") {
  class testee : public event_based_actor {
  public:
    testee(actor_config& cfg, actor buddy)
      : event_based_actor(cfg), buddy_(buddy) {
      attach_functor([this, buddy](const error& reason) {
        mail(ok_atom_v, reason).send(buddy);
      });
    }

    behavior make_behavior() override {
      return {
        [] {
          // nop
        },
      };
    }

    void on_exit() override {
      destroy(buddy_);
    }

  private:
    actor buddy_;
  };
  class spawner : public event_based_actor {
  public:
    spawner(actor_config& cfg)
      : event_based_actor(cfg),
        downs_(0),
        testee_(spawn<testee, monitored>(this)) {
      set_down_handler([this](down_msg& msg) {
        if (msg.reason != exit_reason::user_shutdown)
          CAF_RAISE_ERROR("error is not user_shutdown");
        if (++downs_ == 2)
          quit(msg.reason);
      });
      set_exit_handler(
        [this](exit_msg& msg) { send_exit(testee_, std::move(msg.reason)); });
    }

    behavior make_behavior() override {
      return {
        [this](ok_atom, const error& reason) {
          if (reason != exit_reason::user_shutdown)
            CAF_RAISE_ERROR("error is not user_shutdown");
          if (++downs_ == 2)
            quit(reason);
        },
      };
    }

    void on_exit() override {
      log::test::debug("spawner::on_exit()");
      destroy(testee_);
    }

  private:
    int downs_;
    actor testee_;
  };
  anon_send_exit(system.spawn<spawner>(), exit_reason::user_shutdown);
}

TEST("kill the immortal") {
  auto wannabe_immortal = system.spawn([](event_based_actor* self) -> behavior {
    self->set_exit_handler([](local_actor*, exit_msg&) {
      // nop
    });
    return {
      [] {
        // nop
      },
    };
  });
  scoped_actor self{system};
  self->send_exit(wannabe_immortal, exit_reason::kill);
  self->wait_for(wannabe_immortal);
}

TEST("move-only argument in spawn") {
  using unique_int = std::unique_ptr<int>;
  unique_int uptr{new int(42)};
  auto wrapper = [](event_based_actor* self, unique_int ptr) -> behavior {
    auto i = *ptr;
    return {
      [self, i](float) {
        self->quit();
        return i;
      },
    };
  };
  auto f = make_function_view(system.spawn(wrapper, std::move(uptr)));
  auto received = f(1.f);
  require(received.has_value());
  check_eq(to_tuple<int>(*received), std::make_tuple(42));
}

TEST("move-only function object") {
  struct move_only_fun {
    move_only_fun() = default;
    move_only_fun(const move_only_fun&) = delete;
    move_only_fun(move_only_fun&&) = default;

    behavior operator()(event_based_actor*) {
      return {};
    }
  };
  actor_system_config cfg;
  actor_system sys{cfg};
  move_only_fun f;
  sys.spawn(std::move(f));
}

} // WITH_FIXTURE(fixture)

TEST_INIT() {
  caf::init_global_meta_objects<caf::id_block::dynamic_spawn_test>();
}

} // namespace

CAF_POP_WARNINGS
