// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/scheduled_actor.hpp"

#include "caf/test/test.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/config.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/scoped_actor.hpp"

using namespace caf;

#define ASSERT_COMPILES(expr, msg)                                             \
  static_assert(                                                               \
    std::is_void_v<decltype(std::declval<scheduled_actor*>()->expr)>, msg)

namespace {

// -- compile-time checks for set_default_handler ------------------------------

struct mutable_default_fn {
  skippable_result operator()(message&) {
    return {};
  }
};

ASSERT_COMPILES(set_default_handler(mutable_default_fn{}),
                "set_default_handler must accept mutable function objects");

struct const_default_fn {
  skippable_result operator()(message&) const {
    return {};
  }
};

ASSERT_COMPILES(set_default_handler(const_default_fn{}),
                "set_default_handler must accept const function objects");

// -- compile-time checks for set_error_handler --------------------------------

struct mutable_error_fn {
  void operator()(error&) {
    // nop
  }
};

ASSERT_COMPILES(set_error_handler(mutable_error_fn{}),
                "set_error_handler must accept mutable function objects");

struct const_error_fn {
  void operator()(error&) const {
    // nop
  }
};

ASSERT_COMPILES(set_error_handler(const_error_fn{}),
                "set_error_handler must accept const function objects");

// -- compile-time checks for set_down_handler ---------------------------------

struct mutable_down_fn {
  void operator()(down_msg&) {
    // nop
  }
};

ASSERT_COMPILES(set_down_handler(mutable_down_fn{}),
                "set_down_handler must accept mutable function objects");

struct const_down_fn {
  void operator()(down_msg&) const {
    // nop
  }
};

ASSERT_COMPILES(set_down_handler(const_down_fn{}),
                "set_down_handler must accept const function objects");

// -- compile-time checks for set_node_down_handler ----------------------------

struct mutable_node_down_fn {
  void operator()(node_down_msg&) {
    // nop
  }
};

ASSERT_COMPILES(set_node_down_handler(mutable_node_down_fn{}),
                "set_node_down_handler must accept mutable function objects");

struct const_node_down_fn {
  void operator()(node_down_msg&) const {
    // nop
  }
};

ASSERT_COMPILES(set_node_down_handler(const_node_down_fn{}),
                "set_node_down_handler must accept const function objects");

// -- compile-time checks for set_exit_handler ---------------------------------

struct mutable_exit_fn {
  void operator()(exit_msg&) {
    // nop
  }
};

ASSERT_COMPILES(set_exit_handler(mutable_exit_fn{}),
                "set_exit_handler must accept mutable function objects");

struct const_exit_fn {
  void operator()(exit_msg&) const {
    // nop
  }
};

ASSERT_COMPILES(set_exit_handler(const_exit_fn{}),
                "set_exit_handler must accept const function objects");

// -- compile-time checks for set_exception_handler ----------------------------

#ifdef CAF_ENABLE_EXCEPTIONS

struct mutable_exception_fn {
  error operator()(std::exception_ptr&) {
    return {};
  }
};

ASSERT_COMPILES(set_exception_handler(mutable_exception_fn{}),
                "set_exception_handler must accept mutable function objects");

struct const_exception_fn {
  error operator()(std::exception_ptr&) const {
    return {};
  }
};

ASSERT_COMPILES(set_exception_handler(const_exception_fn{}),
                "set_exception_handler must accept const function objects");

behavior testee() {
  return {
    [](const std::string&) { throw std::runtime_error("whatever"); },
  };
}

behavior exception_testee(event_based_actor* self) {
  self->set_exception_handler([](std::exception_ptr&) -> error {
    return exit_reason::remote_link_unreachable;
  });
  return testee();
}

TEST("the default exception handler includes the error message") {
  using std::string;
  actor_system_config cfg;
  actor_system system{cfg};
  scoped_actor self{system};
  auto aut = self->spawn(testee);
  self->mail("hello world")
    .request(aut, infinite)
    .receive( //
      [this] { fail("unexpected response"); },
      [this](const error& err) {
        check_eq(err.what(),
                 "unhandled exception of type std.runtime_error: whatever");
      });
}

TEST("actors can override the default exception handler") {
  actor_system_config cfg;
  actor_system system{cfg};
  auto handler = [](std::exception_ptr& eptr) -> error {
    try {
      std::rethrow_exception(eptr);
    } catch (std::runtime_error&) {
      return exit_reason::normal;
    } catch (...) {
      // "fall through"
    }
    return sec::runtime_error;
  };
  scoped_actor self{system};
  auto testee1 = self->spawn([=](event_based_actor* eb_self) {
    eb_self->set_exception_handler(handler);
    throw std::runtime_error("ping");
  });
  auto testee2 = self->spawn([=](event_based_actor* eb_self) {
    eb_self->set_exception_handler(handler);
    throw std::logic_error("pong");
  });
  auto testee3 = self->spawn(exception_testee);
  self->mail("foo").send(testee3);
  // receive all down messages
  self->wait_for(testee1, testee2, testee3);
}

#endif // CAF_ENABLE_EXCEPTIONS

} // namespace
