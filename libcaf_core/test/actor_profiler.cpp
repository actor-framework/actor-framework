/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE actor_profiler

#include "caf/actor_profiler.hpp"

#include "core-test.hpp"

#include "caf/config.hpp"

#ifdef CAF_ENABLE_ACTOR_PROFILER

using namespace caf;

namespace {

using string_list = std::vector<std::string>;

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
    str += to_string(result);
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

CAF_TEST_FIXTURE_SCOPE(actor_profiler_tests, fixture)

CAF_TEST(profilers record actor construction) {
  CAF_MESSAGE("fully initialize CAF, ignore system-internal actors");
  run();
  rec.log.clear();
  CAF_MESSAGE("spawn a foo and a bar");
  auto bar = [](stateful_actor<bar_state>*) {};
  auto foo = [bar](stateful_actor<foo_state>* self) { self->spawn(bar); };
  auto foo_actor = sys.spawn(foo);
  run();
  foo_actor = nullptr;
  CAF_CHECK_EQUAL(string_list({
                    "new: foo",
                    "new: bar, parent: foo",
                    "delete: bar",
                    "delete: foo",
                  }),
                  rec.log);
}

CAF_TEST(profilers record asynchronous messaging) {
  CAF_MESSAGE("fully initialize CAF, ignore system-internal actors");
  run();
  rec.log.clear();
  CAF_MESSAGE("spawn a foo and a bar");
  auto bar = [](stateful_actor<bar_state>*) -> behavior {
    return {
      [](const std::string& str) {
        CAF_CHECK_EQUAL(str, "hello bar");
        return "hello foo";
      },
    };
  };
  auto foo = [bar](stateful_actor<foo_state>* self) -> behavior {
    auto b = self->spawn(bar);
    self->send(b, "hello bar");
    return {
      [](const std::string& str) { CAF_CHECK_EQUAL(str, "hello foo"); },
    };
  };
  sys.spawn(foo);
  run();
  CAF_CHECK_EQUAL(string_list({
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
  CAF_MESSAGE("fully initialize CAF, ignore system-internal actors");
  run();
  rec.log.clear();
  CAF_MESSAGE("spawn a client and a server with one worker");
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
      CAF_CHECK_EQUAL(result, 42);
    });
  };
  sys.spawn(client, sys.spawn(server, sys.spawn(worker)));
  run();
  for (const auto& line : rec.log) {
    CAF_MESSAGE(line);
  }
  CAF_CHECK_EQUAL(string_list({
                    "new: worker",
                    "new: server",
                    "new: client",
                    "client sends: message(int32_t(19), int32_t(23))",
                    "server got: message(int32_t(19), int32_t(23))",
                    "server sends: message(int32_t(19), int32_t(23))",
                    "server consumed the message",
                    "delete: server",
                    "worker got: message(int32_t(19), int32_t(23))",
                    "worker sends: message(int32_t(42))",
                    "worker consumed the message",
                    "client got: message(int32_t(42))",
                    "client consumed the message",
                    "delete: worker",
                    "delete: client",
                  }),
                  rec.log);
}

CAF_TEST_FIXTURE_SCOPE_END()

#endif // CAF_ENABLE_ACTOR_PROFILER
