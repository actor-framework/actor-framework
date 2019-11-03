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

#define CAF_SUITE tracing_data

#include "caf/tracing_data.hpp"

#include "caf/test/dsl.hpp"

#include <vector>

#include "caf/actor_profiler.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/config.hpp"
#include "caf/serializer_impl.hpp"
#include "caf/tracing_data_factory.hpp"

#ifdef CAF_ENABLE_ACTOR_PROFILER

using std::string;

using namespace caf;

namespace {

class dummy_tracing_data : public tracing_data {
public:
  string value;

  dummy_tracing_data(string value) : value(std::move(value)) {
    // nop
  }

  error serialize(serializer& sink) const override {
    return sink(value);
  }
};

class dummy_tracing_data_factory : public tracing_data_factory {
public:
  error deserialize(deserializer& source,
                    std::unique_ptr<tracing_data>& dst) const override {
    string value;
    if (auto err = source(value))
      return err;
    dst.reset(new dummy_tracing_data(std::move(value)));
    return none;
  }
};

class dummy_profiler : public actor_profiler {
public:
  void add_actor(const local_actor&, const local_actor*) override {
    // nop
  }

  void remove_actor(const local_actor&) override {
    // nop
  }

  void before_processing(const local_actor&, const mailbox_element&) override {
    // nop
  }

  void after_processing(const local_actor&, invoke_message_result) override {
    // nop
  }

  void before_sending(const local_actor& self,
                      mailbox_element& element) override {
    element.tracing_id.reset(new dummy_tracing_data(self.name()));
  }

  void before_sending_scheduled(const local_actor& self,
                                actor_clock::time_point,
                                mailbox_element& element) override {
    element.tracing_id.reset(new dummy_tracing_data(self.name()));
  }
};

actor_system_config& init(actor_system_config& cfg, actor_profiler& profiler,
                          tracing_data_factory& factory) {
  test_coordinator_fixture<>::init_config(cfg);
  cfg.profiler = &profiler;
  cfg.tracing_context = &factory;
  return cfg;
}

struct fixture {
  using scheduler_type = caf::scheduler::test_coordinator;

  fixture()
    : sys(init(cfg, profiler, factory)),
      sched(dynamic_cast<scheduler_type&>(sys.scheduler())) {
    run();
  }

  void run() {
    sched.run();
  }

  dummy_profiler profiler;
  dummy_tracing_data_factory factory;
  actor_system_config cfg;
  actor_system sys;
  scheduler_type& sched;
};

const std::string& tracing_id(local_actor* self) {
  auto element = self->current_mailbox_element();
  if (element == nullptr)
    CAF_FAIL("current_mailbox_element == null");
  auto tid = element->tracing_id.get();
  if (tid == nullptr)
    CAF_FAIL("tracing_id == null");
  auto dummy_tid = dynamic_cast<dummy_tracing_data*>(tid);
  if (dummy_tid == nullptr)
    CAF_FAIL("dummy_tracing_id == null");
  return dummy_tid->value;
}

#  define NAMED_ACTOR_STATE(type)                                              \
    struct type##_state {                                                      \
      const char* name = #type;                                                \
    }

NAMED_ACTOR_STATE(alice);
NAMED_ACTOR_STATE(bob);
NAMED_ACTOR_STATE(carl);

} // namespace

CAF_TEST_FIXTURE_SCOPE(actor_profiler_tests, fixture)

CAF_TEST(profilers inject tracing data into asynchronous messages) {
  CAF_MESSAGE("spawn a foo and a bar");
  auto carl_fun = [](stateful_actor<carl_state>* self) -> behavior {
    return {
      [=](const string& str) {
        CAF_CHECK_EQUAL(str, "hello carl");
        CAF_CHECK_EQUAL(tracing_id(self), "bob");
      },
    };
  };
  auto bob_fun = [](stateful_actor<bob_state>* self, actor carl) -> behavior {
    return {
      [=](const string& str) {
        CAF_CHECK_EQUAL(str, "hello bob");
        CAF_CHECK_EQUAL(tracing_id(self), "alice");
        self->send(carl, "hello carl");
      },
    };
  };
  auto alice_fun = [](stateful_actor<alice_state>* self, actor bob) {
    self->send(bob, "hello bob");
  };
  sys.spawn(alice_fun, sys.spawn(bob_fun, sys.spawn(carl_fun)));
  run();
}

CAF_TEST(tracing data is serializable) {
  std::vector<byte> buf;
  serializer_impl<std::vector<byte>> sink{sys, buf};
  tracing_data_ptr data;
  tracing_data_ptr copy;
  data.reset(new dummy_tracing_data("iTrace"));
  CAF_CHECK_EQUAL(inspect(sink, data), none);
  binary_deserializer source{sys, buf};
  CAF_CHECK_EQUAL(inspect(source, copy), none);
  CAF_REQUIRE_NOT_EQUAL(copy.get(), nullptr);
  CAF_CHECK_EQUAL(dynamic_cast<dummy_tracing_data&>(*copy).value, "iTrace");
}

CAF_TEST_FIXTURE_SCOPE_END()

#endif // CAF_ENABLE_ACTOR_PROFILER
