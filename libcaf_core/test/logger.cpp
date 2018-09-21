/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/config.hpp"

#define CAF_SUITE logger
#include "caf/test/unit_test.hpp"

#include <ctime>
#include <string>

#include "caf/all.hpp"

using namespace caf;
using namespace std;
using namespace std::chrono;

namespace {

struct fixture {
  fixture() {
    cfg.set("scheduler.policy", atom("testing"));
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
    ostringstream oss;
    f(oss, forward<Ts>(xs)...);
    return oss.str();
  }

  actor_system_config cfg;
  logger::line_format lf;
};

constexpr const char* file_format = "%r %c %p %a %t %C %M %F:%L %m%n";

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(logger_tests, fixture)

// copy construction, copy assign, move construction, move assign
// and finally serialization round-trip
CAF_TEST(parse_default_format_strings) {
  actor_system sys{cfg};
  add(logger::runtime_field);
  add(logger::plain_text_field, " ");
  add(logger::category_field);
  add(logger::plain_text_field, " ");
  add(logger::priority_field);
  add(logger::plain_text_field, " ");
  add(logger::actor_field);
  add(logger::plain_text_field, " ");
  add(logger::thread_field);
  add(logger::plain_text_field, " ");
  add(logger::class_name_field);
  add(logger::plain_text_field, " ");
  add(logger::method_field);
  add(logger::plain_text_field, " ");
  add(logger::file_field);
  add(logger::plain_text_field, ":");
  add(logger::line_field);
  add(logger::plain_text_field, " ");
  add(logger::message_field);
  add(logger::newline_field);
  CAF_CHECK_EQUAL(logger::parse_format(file_format), lf);
#if CAF_LOG_LEVEL >= 0
  // Not parsed when compiling without logging enabled.
  CAF_CHECK_EQUAL(sys.logger().file_format(), lf);
#endif
}

CAF_TEST(rendering) {
  // Rendering of type names and function names.
  const char* foobar = "void ns::foo::bar()";
  CAF_CHECK_EQUAL(render(logger::render_fun_name, foobar), "bar");
  CAF_CHECK_EQUAL(render(logger::render_fun_prefix, foobar), "ns.foo");
  // Rendering of time points.
  timestamp t0;
  timestamp t1{timestamp::duration{5000000}}; // epoch + 5000000ns (5ms)
  CAF_CHECK_EQUAL(render(logger::render_time_diff, t0, t1), "5");
  time_t t0_t = 0;
  char t0_buf[50];
  CAF_REQUIRE(strftime(t0_buf, sizeof(t0_buf),
                       "%Y-%m-%dT%H:%M:%S.000", localtime(&t0_t)));
  CAF_CHECK_EQUAL(render(logger::render_date, t0), t0_buf);
  // Rendering of events.
  logger::event e{
    CAF_LOG_LEVEL_WARNING,
    "unit.test",
    "void ns::foo::bar()",
    "foo.cpp",
    42,
    "hello world",
    this_thread::get_id(),
    0,
    t0
  };
  // Exclude %r and %t from rendering test because they are nondeterministic.
  actor_system sys{cfg};
  auto lf = logger::parse_format("%c %p %a %C %M %F:%L %m");
  auto& lg = sys.logger();
  using namespace std::placeholders;
  auto render_event = bind(&logger::render, &lg, _1, _2, _3);
  CAF_CHECK_EQUAL(render(render_event, lf, e),
                  "unit.test WARN actor0 ns.foo bar foo.cpp:42 hello world");
}

CAF_TEST_FIXTURE_SCOPE_END()
