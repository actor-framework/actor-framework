// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE actor_profiler

#include "caf/actor_profiler.hpp"

#include "core-test.hpp"

#include "caf/config.hpp"

#ifdef CAF_ENABLE_ACTOR_PROFILER

using namespace caf;

namespace {

using string_list = std::vector<std::string>;

std::string short_string(invoke_message_result result) {
  auto str = to_string(result);
  string_list components;
  split(components, str, "::", false);
  if (components.empty())
    return str;
  else
    return std::move(components.back());
}

struct recorder : actor_profiler {
  void add_actor(const local_actor& self, const local_actor* parent) override {
    log.emplace_back("new: ");
    auto& str = log.back();
    str += self.name();
    if (parent != nullptr) {
      str += ", parent: ";
      str += parent->name();
    }
  }

  void remove_actor(const local_actor& self) override {
    log.emplace_back("delete: ");
    log.back() += self.name();
  }

  void before_processing(const local_actor& self,
                         const mailbox_element& element) override {
    log.emplace_back(self.name());
    auto& str = log.back();
    str += " got: ";
    str += to_string(element.content());
  }

  void after_processing(const local_actor& self,
                        invoke_message_result result) override {
    log.emplace_back(self.name());
    auto& str = log.back();
    str += " ";
    str += short_string(result);
    str += " the message";
  }

  void before_sending(const local_actor& self,
                      mailbox_element& element) override {
    log.emplace_back(self.name());
    auto& str = log.back();
    str += " sends: ";
    str += to_string(element.content());
  }

  void before_sending_scheduled(const local_actor& self,
                                actor_clock::time_point,
                                mailbox_element& element) override {
    log.emplace_back(self.name());
    auto& str = log.back();
    str += " sends (scheduled): ";
    str += to_string(element.content());
  }

  string_list log;
};

actor_system_config& init(actor_system_config& cfg, recorder& rec) {
  test_coordinator_fixture<>::init_config(cfg);
  cfg.profiler = &rec;
  return cfg;
}

struct fixture {
  using scheduler_type = caf::scheduler::test_coordinator;

  fixture()
    : sys(init(cfg, rec)),
      sched(dynamic_cast<scheduler_type&>(sys.scheduler())) {
    // nop
  }

  void run() {
    sched.run();
  }

  recorder rec;
  actor_system_config cfg;
  actor_system sys;
  scheduler_type& sched;
};

#  define NAMED_ACTOR_STATE(type)                                              \
    struct type##_state {                                                      \
      static inline const char* name = #type;                                  \
    }

NAMED_ACTOR_STATE(bar);
NAMED_ACTOR_STATE(client);
NAMED_ACTOR_STATE(foo);
NAMED_ACTOR_STATE(server);
NAMED_ACTOR_STATE(worker);

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(profilers record actor construction) {
  MESSAGE("fully initialize CAF, ignore system-internal actors");
  run();
  rec.log.clear();
  MESSAGE("spawn a foo and a bar");
  auto bar = [](stateful_actor<bar_state>*) {};
  auto foo = [bar](stateful_actor<foo_state>* self) { self->spawn(bar); };
  auto foo_actor = sys.spawn(foo);
  run();
  foo_actor = nullptr;
  CHECK_EQ(string_list({
             "new: foo",
             "new: bar, parent: foo",
             "delete: bar",
             "delete: foo",
           }),
           rec.log);
}

CAF_TEST(profilers record asynchronous messaging) {
  MESSAGE("fully initialize CAF, ignore system-internal actors");
  run();
  rec.log.clear();
  MESSAGE("spawn a foo and a bar");
  auto bar = [](stateful_actor<bar_state>*) -> behavior {
    return {
      [](const std::string& str) {
        CHECK_EQ(str, "hello bar");
        return "hello foo";
      },
    };
  };
  auto foo = [bar](stateful_actor<foo_state>* self) -> behavior {
    auto b = self->spawn(bar);
    self->send(b, "hello bar");
    return {
      [](const std::string& str) { CHECK_EQ(str, "hello foo"); },
    };
  };
  sys.spawn(foo);
  run();
  CHECK_EQ(string_list({
             R"__(new: foo)__",
             R"__(new: bar, parent: foo)__",
             R"__(foo sends: message("hello bar"))__",
             R"__(bar got: message("hello bar"))__",
             R"__(bar sends: message("hello foo"))__",
             R"__(bar consumed the message)__",
             R"__(foo got: message("hello foo"))__",
             R"__(delete: bar)__",
             R"__(foo consumed the message)__",
             R"__(delete: foo)__",
           }),
           rec.log);
}

CAF_TEST(profilers record request / response messaging) {
  MESSAGE("fully initialize CAF, ignore system-internal actors");
  run();
  rec.log.clear();
  MESSAGE("spawn a client and a server with one worker");
  auto worker = [](stateful_actor<worker_state>*) -> behavior {
    return {
      [](int x, int y) { return x + y; },
    };
  };
  auto server = [](stateful_actor<server_state>* self, actor work) -> behavior {
    return {
      [=](int x, int y) { return self->delegate(work, x, y); },
    };
  };
  auto client = [](stateful_actor<client_state>* self, actor serv) {
    self->request(serv, infinite, 19, 23).then([](int result) {
      CHECK_EQ(result, 42);
    });
  };
  sys.spawn(client, sys.spawn(server, sys.spawn(worker)));
  run();
  for (const auto& line : rec.log) {
    MESSAGE(line);
  }
  CHECK_EQ(string_list({
             "new: worker",
             "new: server",
             "new: client",
             "client sends: message(19, 23)",
             "server got: message(19, 23)",
             "server sends: message(19, 23)",
             "server consumed the message",
             "delete: server",
             "worker got: message(19, 23)",
             "worker sends: message(42)",
             "worker consumed the message",
             "client got: message(42)",
             "client consumed the message",
             "delete: worker",
             "delete: client",
           }),
           rec.log);
}

END_FIXTURE_SCOPE()

#endif // CAF_ENABLE_ACTOR_PROFILER
