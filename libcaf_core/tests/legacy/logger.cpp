// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE logger

#include "caf/logger.hpp"

#include "caf/all.hpp"

#include "core-test.hpp"

#include <ctime>
#include <string>

using namespace caf;
using namespace std::literals;

using std::string;

#define CHECK_FUN_PREFIX(PrefixName)                                           \
  do {                                                                         \
    auto e = CAF_LOG_MAKE_EVENT(0, "caf", CAF_LOG_LEVEL_DEBUG, "");            \
    std::ostringstream oss;                                                    \
    ::caf::logger::render_fun_prefix(oss, e);                                  \
    auto prefix = oss.str();                                                   \
    if (prefix != PrefixName)                                                  \
      CAF_ERROR("rendering the prefix of " << e.pretty_fun << " produced "     \
                                           << prefix << " instead of "         \
                                           << PrefixName);                     \
    else                                                                       \
      CHECK_EQ(prefix, PrefixName);                                            \
  } while (false)

void global_fun() {
  CHECK_FUN_PREFIX("GLOBAL");
}

// Little helper that allows us to write a single check for all compilers.
// Clang expands template parameters in __PRETTY_FUNCTION__, while GCC does
// not. For example, Clang would produce "void foo<int>::bar()", while GCC
// would produce "void foo<T>::bar() [with T = int]". A type called T gives
// us always the same output for the prefix.
struct T {};

namespace {

struct fixture {
  fixture() {
    cfg.set("caf.scheduler.policy", "testing");
    cfg.set("caf.logger.file.verbosity", "debug");
    cfg.set("caf.logger.file.path", "");
  }

  void add(logger::field_type kind) {
    lf.emplace_back(logger::field{kind, std::string{}});
  }

  template <size_t N>
  void add(logger::field_type kind, const char (&str)[N]) {
    lf.emplace_back(logger::field{kind, std::string{str, str + (N - 1)}});
  }

  template <class F, class... Ts>
  string render(F f, Ts&&... xs) {
    std::ostringstream oss;
    f(oss, std::forward<Ts>(xs)...);
    return oss.str();
  }

  actor_system_config cfg;
  logger::line_format lf;
};

void fun() {
  CHECK_FUN_PREFIX("$");
}

const char* ptr_fun(const char* x) {
  CHECK_FUN_PREFIX("$");
  return x;
}

const int& ref_fun(const int& x) {
  CHECK_FUN_PREFIX("$");
  return x;
}

template <class T>
struct tpl {
  static void run() {
    CHECK_FUN_PREFIX("$.tpl<T>");
  }
};

namespace foo {

void fun() {
  CHECK_FUN_PREFIX("$.foo");
}

const char* ptr_fun(const char* x) {
  CHECK_FUN_PREFIX("$.foo");
  return x;
}

const int& ref_fun(const int& x) {
  CHECK_FUN_PREFIX("$.foo");
  return x;
}

template <class T>
struct tpl {
  static void run() {
    CHECK_FUN_PREFIX("$.foo.tpl<T>");
  }
};

} // namespace foo

constexpr const char* file_format = "%r %c %p %a %t %C %M %F:%L %m%n";

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(rendering) {
  // Rendering of time points.
  timestamp t0;
  time_t t0_t = 0;
  char t0_buf[50];
  strftime(t0_buf, sizeof(t0_buf), "%Y-%m-%dT%H:%M:%S", localtime(&t0_t));
  // Rendering of events.
  logger::event e{
    CAF_LOG_LEVEL_WARNING,
    42,
    "unit_test",
    "void ns::foo::bar()",
    "bar",
    "foo.cpp",
    "hello world",
    std::this_thread::get_id(),
    0,
    t0,
  };
  CHECK_EQ(render(logger::render_fun_prefix, e), "ns.foo"sv);
}

CAF_TEST(render_fun_prefix) {
  int i = 42;
  global_fun();
  fun();
  ptr_fun(nullptr);
  ref_fun(i);
  tpl<T>::run();
  foo::fun();
  foo::ptr_fun(nullptr);
  foo::ref_fun(i);
  foo::tpl<T>::run();
}

END_FIXTURE_SCOPE()
