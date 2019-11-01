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

#include "caf/test/dsl.hpp"

#include "caf/config.hpp"

#ifdef CAF_ENABLE_ACTOR_PROFILER

using namespace caf;

namespace {

using string_list = std::vector<std::string>;

struct recorder : actor_profiler {
  void add_actor(const local_actor& self, const local_actor* parent) {
    log.emplace_back("new: ");
    auto& str = log.back();
    str += self.name();
    if (parent != nullptr) {
      str += ", parent: ";
      str += parent->name();
    }
  }

  void remove_actor(const local_actor& self) {
    log.emplace_back("delete: ");
    log.back() += self.name();
  }

  void before_processing(const local_actor& self,
                         const mailbox_element& element) {
    log.emplace_back(self.name());
    auto& str = log.back();
    str += " got: ";
    str += to_string(element.content());
  }

  void after_processing(const local_actor& self, invoke_message_result result) {
    log.emplace_back(self.name());
    auto& str = log.back();
    str += " ";
    str += to_string(result);
    str += " the message";
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

struct foo_state {
  const char* name = "foo";
};

struct bar_state {
  const char* name = "bar";
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(actor_profiler_tests, fixture)

CAF_TEST(record actor construction) {
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

CAF_TEST(record actor messaging) {
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
                    "new: foo",
                    "new: bar, parent: foo",
                    "bar got: (\"hello bar\")",
                    "bar consumed the message",
                    "foo got: (\"hello foo\")",
                    "delete: bar",
                    "foo consumed the message",
                    "delete: foo",
                  }),
                  rec.log);
}

CAF_TEST_FIXTURE_SCOPE_END()

#endif // CAF_ENABLE_ACTOR_PROFILER
