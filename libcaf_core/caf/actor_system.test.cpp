// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/actor_system.hpp"

#include "caf/test/test.hpp"

#include "caf/actor_system_config.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/scoped_actor.hpp"

#include <memory>

using namespace caf;

namespace {

using shared_bool_ptr = std::shared_ptr<bool>;

class dummy_actor : public event_based_actor {
public:
  using super = event_based_actor;

  dummy_actor(actor_config& cfg, shared_bool_ptr flag)
    : super(cfg), flag_(flag) {
    // nop
  }

  void launch(execution_unit* eu, bool lazy, bool hide) override {
    *flag_ = true;
    super::launch(eu, lazy, hide);
  }

private:
  shared_bool_ptr flag_;
};

TEST("spawn_inactive creates an actor without launching it") {
  auto flag = std::make_shared<bool>(false);
  actor_system_config cfg;
  put(cfg.content, "caf.scheduler.max-threads", 1);
  actor_system sys{cfg};
  SECTION("users may launch the actor manually") {
    auto [self, launch] = sys.spawn_inactive<dummy_actor>(flag);
    check_eq(self->ctrl()->strong_refs, 1u); // 1 ref by launch
    check(!*flag);
    launch();
    check(*flag);
    SECTION("calling launch() twice is a no-op") {
      *flag = false;
      launch();
      check(!*flag);
    }
  }
  SECTION("the actor launches automatically at scope exit") {
    {
      auto [self, launch] = sys.spawn_inactive<dummy_actor>(flag);
      check_eq(self->ctrl()->strong_refs, 1u); // 1 ref by launch
      check(!*flag);
    }
    sys.await_all_actors_done();
    check(*flag);
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

} // namespace
