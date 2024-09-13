// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/actor_system.hpp"

#include "caf/test/test.hpp"

#include "caf/actor_registry.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/scoped_actor.hpp"

#include <memory>

using namespace caf;

using shared_bool_ptr = std::shared_ptr<bool>;

TEST("spawn_inactive creates an actor without launching it") {
  actor_system_config cfg;
  put(cfg.content, "caf.scheduler.max-threads", 1);
  actor_system sys{cfg};
  SECTION("users may launch the actor manually") {
    auto baseline = sys.registry().running();
    auto [self, launch] = sys.spawn_inactive();
    auto strong_self = actor{self};
    self->become([](int) {});
    check_eq(sys.registry().running(), baseline);
    launch();
    check_eq(sys.registry().running(), baseline + 1);
    SECTION("calling launch() twice is a no-op") {
      launch();
      check_eq(sys.registry().running(), baseline + 1);
    }
  }
  SECTION("the actor launches automatically at scope exit") {
    auto baseline = sys.registry().running();
    auto strong_self = actor{};
    {
      auto [self, launch] = sys.spawn_inactive();
      check_eq(sys.registry().running(), baseline);
      self->become([](int) {});
      check_eq(self->ctrl()->strong_refs, 1u); // 1 ref by launch
      strong_self = actor{self};
    }
    check_eq(sys.registry().running(), baseline + 1);
    strong_self = nullptr;
    sys.await_all_actors_done();
    check_eq(sys.registry().running(), baseline);
  }
  // Note: checking the ref count at the end to verify that `launch` has dropped
  //       its reference to the actor is unreliable, because the scheduler holds
  //       on to a reference as well that may not be dropped yet.
}

TEST("println renders its arguments to a text stream") {
  actor_system_config cfg;
  put(cfg.content, "caf.scheduler.max-threads", 1);
  actor_system sys{cfg};
  auto out = new std::string;
  auto write = [](void* vptr, term color, const char* buf, size_t len) {
    if (len == 0)
      return;
    auto* str = static_cast<std::string*>(vptr);
    if (color <= term::reset_endl) {
      str->insert(str->end(), buf, buf + len);
    } else {
      auto has_nl = false;
      *str += detail::format("<{}>", to_string(color));
      str->insert(str->end(), buf, buf + len);
      if (str->back() == '\n') {
        str->pop_back();
        has_nl = true;
      }
      *str += detail::format("</{}>", to_string(color));
      if (has_nl)
        *str += '\n';
    }
  };
  auto cleanup = [](void* vptr) { delete static_cast<std::string*>(vptr); };
  sys.redirect_text_output(out, write, cleanup);
  sys.println("line1");
  sys.println(term::red, "line{}", 2);
  sys.spawn([](event_based_actor* self) {
    self->println("line{}", 3);
    self->println(term::green, "line{}", 4);
  });
  sys.await_all_actors_done();
  check_eq(*out, "line1\n<red>line2</red>\nline3\n<green>line4</green>\n");
}
