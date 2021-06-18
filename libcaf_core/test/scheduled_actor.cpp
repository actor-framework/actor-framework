// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE scheduled_actor

#include "caf/scheduled_actor.hpp"

#include "caf/test/dsl.hpp"

using namespace caf;

#define ASSERT_COMPILES(expr, msg)                                             \
  static_assert(std::is_void_v<decltype(expr)>, msg);

namespace {

constexpr scheduled_actor* nil_actor = nullptr;

// -- compile-time checks for set_default_handler ------------------------------

struct mutable_default_fn {
  skippable_result operator()(message&) {
    return {};
  }
};

ASSERT_COMPILES(nil_actor->set_default_handler(mutable_default_fn{}),
                "set_default_handler must accept mutable function objects");

struct const_default_fn {
  skippable_result operator()(message&) const {
    return {};
  }
};

ASSERT_COMPILES(nil_actor->set_default_handler(const_default_fn{}),
                "set_default_handler must accept const function objects");

// -- compile-time checks for set_error_handler --------------------------------

struct mutable_error_fn {
  void operator()(error&) {
    // nop
  }
};

ASSERT_COMPILES(nil_actor->set_error_handler(mutable_error_fn{}),
                "set_error_handler must accept mutable function objects");

struct const_error_fn {
  void operator()(error&) const {
    // nop
  }
};

ASSERT_COMPILES(nil_actor->set_error_handler(const_error_fn{}),
                "set_error_handler must accept const function objects");

// -- compile-time checks for set_down_handler ---------------------------------

struct mutable_down_fn {
  void operator()(down_msg&) {
    // nop
  }
};

ASSERT_COMPILES(nil_actor->set_down_handler(mutable_down_fn{}),
                "set_down_handler must accept mutable function objects");

struct const_down_fn {
  void operator()(down_msg&) const {
    // nop
  }
};

ASSERT_COMPILES(nil_actor->set_down_handler(const_down_fn{}),
                "set_down_handler must accept const function objects");

// -- compile-time checks for set_node_down_handler ----------------------------

struct mutable_node_down_fn {
  void operator()(node_down_msg&) {
    // nop
  }
};

ASSERT_COMPILES(nil_actor->set_node_down_handler(mutable_node_down_fn{}),
                "set_node_down_handler must accept mutable function objects");

struct const_node_down_fn {
  void operator()(node_down_msg&) const {
    // nop
  }
};

ASSERT_COMPILES(nil_actor->set_node_down_handler(const_node_down_fn{}),
                "set_node_down_handler must accept const function objects");

// -- compile-time checks for set_exit_handler ---------------------------------

struct mutable_exit_fn {
  void operator()(exit_msg&) {
    // nop
  }
};

ASSERT_COMPILES(nil_actor->set_exit_handler(mutable_exit_fn{}),
                "set_exit_handler must accept mutable function objects");

struct const_exit_fn {
  void operator()(exit_msg&) const {
    // nop
  }
};

ASSERT_COMPILES(nil_actor->set_exit_handler(const_exit_fn{}),
                "set_exit_handler must accept const function objects");

// -- compile-time checks for set_exception_handler ----------------------------

#ifdef CAF_ENABLE_EXCEPTIONS

struct mutable_exception_fn {
  error operator()(std::exception_ptr&) {
    return {};
  }
};

ASSERT_COMPILES(nil_actor->set_exception_handler(mutable_exception_fn{}),
                "set_exception_handler must accept mutable function objects");

struct const_exception_fn {
  error operator()(std::exception_ptr&) const {
    return {};
  }
};

ASSERT_COMPILES(nil_actor->set_exception_handler(const_exception_fn{}),
                "set_exception_handler must accept const function objects");

#endif // CAF_ENABLE_EXCEPTIONS

} // namespace
