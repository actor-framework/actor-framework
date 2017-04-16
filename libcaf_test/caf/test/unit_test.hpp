/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_TEST_UNIT_TEST_HPP
#define CAF_TEST_UNIT_TEST_HPP

#include <map>
#include <cmath>
#include <mutex>
#include <thread>
#include <vector>
#include <string>
#include <memory>
#include <fstream>
#include <sstream>
#include <iostream>

#include "caf/fwd.hpp"
#include "caf/term.hpp"
#include "caf/optional.hpp"

#include "caf/deep_to_string.hpp"

namespace caf {
namespace test {

// -- Function objects for implementing CAF_CHECK_* macros ---------------------

template <class F>
struct negated {
  template <class T, class U>
  bool operator()(const T& x, const U& y) {
    F f;
    return !f(x, y);
  }
};

struct equal_to {
  template <class T, class U,
            typename std::enable_if<std::is_floating_point<T>::value
                                    || std::is_floating_point<U>::value,
                                    int>::type = 0>
  bool operator()(const T& t, const U& u) {
    auto x = static_cast<long double>(t);
    auto y = static_cast<long double>(u);
    auto max = std::max(std::abs(x), std::abs(y));
    auto dif = std::abs(x - y);
    return dif <= max * 1e-5l;
  }

  template <class T, class U,
            typename std::enable_if<!std::is_floating_point<T>::value
                                    && !std::is_floating_point<U>::value,
                                    int>::type = 0>
  bool operator()(const T& x, const U& y) {
    return x == y;
  }
};

// note: we could use negated<equal_to>, but that would give us `!(x == y)`
// instead of `x != y` and thus messes with coverage testing
struct not_equal_to {
  template <class T, class U>
  bool operator()(const T& x, const U& y) {
    return x != y;
  }
};

struct less_than {
  template <class T, class U>
  bool operator()(const T& x, const U& y) {
    return x < y;
  }
};

struct less_than_or_equal {
  template <class T, class U>
  bool operator()(const T& x, const U& y) {
    return x <= y;
  }
};

struct greater_than {
  template <class T, class U>
  bool operator()(const T& x, const U& y) {
    return x > y;
  }
};

struct greater_than_or_equal {
  template <class T, class U>
  bool operator()(const T& x, const U& y) {
    return x >= y;
  }
};

// -- Core components of the unit testing abstraction --------------------------

/// Default test-running function.
/// This function will be called automatically unless you define
/// `CAF_TEST_NO_MAIN` before including `caf/test/unit_test.hpp`. In
/// the latter case you will have to provide you own `main` function,
/// where you may want to call `caf::test::main` from.
int main(int argc, char** argv);

/// A sequence of *checks*.
class test {
public:
  test(std::string test_name);

  virtual ~test();

  size_t expected_failures() const;

  void pass();

  void fail(bool expected);

  const std::string& name() const;

  inline size_t good() {
    return good_;
  }

  inline size_t bad() {
    return bad_;
  }

  virtual void run() = 0;

private:
  size_t expected_failures_;
  std::string name_;
  size_t good_;
  size_t bad_;
};

struct dummy_fixture { };

template <class T>
class test_impl : public test {
public:
  test_impl(std::string test_name) : test(std::move(test_name)) {
    // nop
  }

  void run() override {
    T impl;
    impl.run();
  }
};

namespace detail {

[[noreturn]] void requirement_failed(const std::string& msg);

// constructs spacing given a line number.
const char* fill(size_t line);

void remove_trailing_spaces(std::string& x);

} // namespace detail

/// Logs messages for the test framework.
class logger {
public:
  enum class level : int {
    quiet   = 0,
    error   = 1,
    info    = 2,
    verbose = 3,
    massive = 4
  };

  static bool init(int lvl_cons, int lvl_file, const std::string& logfile);

  static logger& instance();

  template <class T>
  void log(level lvl, const T& x) {
    if (lvl <= level_console_) {
      *console_ << x;
    }
    if (lvl <= level_file_) {
      file_ << x;
    }
  }

  /// Output stream for logging purposes.
  class stream {
  public:
    stream(logger& parent, level lvl);

    stream(const stream&) = default;

    template <class T>
    stream& operator<<(const T& x) {
      parent_.log(lvl_, x);
      return *this;
    }

    template <class T>
    stream& operator<<(const optional<T>& x) {
      if (!x)
        return *this << "-none-";
      return *this << *x;
    }

  private:
    logger& parent_;
    level lvl_;
  };

  stream error();
  stream info();
  stream verbose();
  stream massive();

  void disable_colors();

private:
  logger();

  level level_console_;
  level level_file_;
  std::ostream* console_;
  std::ofstream file_;
  std::ostringstream dummy_;
};

/// Drives unit test execution.
class engine {
public:
  /// Sets external command line arguments.
  /// @param argc The argument counter.
  /// @param argv The argument vectors.
  static void args(int argc, char** argv);

  /// Retrieves the argument counter.
  /// @returns The number of arguments set via ::args or 0.
  static int argc();

  /// Retrieves the argument vector.
  /// @returns The argument vector set via ::args or `nullptr`.
  static char** argv();

  /// Sets path of current executable.
  /// @param argv The path of current executable.
  static void path(char* argv);

  /// Retrieves the path of current executable
  /// @returns The path to executable set via ::path(char*) or `nullptr`.
  static char* path();

  /// Returns the maximum number of seconds a test case is allowed to run.
  static int max_runtime();

  /// Sets the maximum number of seconds a test case is
  /// allowed to run to `value`.
  static void max_runtime(int value);

  /// Adds a test to the engine.
  /// @param cstr_name The name of the suite.
  /// @param ptr The test to register.
  static void add(const char* cstr_name, std::unique_ptr<test> ptr);

  /// Invokes tests in all suites.
  /// @param colorize Whether to colorize the output.
  /// @param log_file The filename of the log output. The empty string means
  ///                 that no log file will be written.
  /// @param verbosity_console The log verbosity level on the console.
  /// @param verbosity_file The log verbosity level in the log file.
  /// @param suites_str Regular expression for including test suites.
  /// @param not_suites_str Regular expression for excluding test suites.
  /// @param tests_str Regular expression for individually selecting tests.
  /// @param not_tests_str Regular expression for individually disabling tests.
  /// @returns `true` iff all tests succeeded.
  static bool run(bool colorize,
                  const std::string& log_file,
                  int verbosity_console,
                  int verbosity_file,
                  const std::string& suites_str,
                  const std::string& not_suites_str,
                  const std::string& tests_str,
                  const std::string& not_tests_str);

  static const char* last_check_file();
  static void last_check_file(const char* file);

  static size_t last_check_line();
  static void last_check_line(size_t line);

  static test* current_test();

  static std::vector<std::string> available_suites();

  static std::vector<std::string> available_tests(const std::string& suite);

private:
  engine() = default;

  static engine& instance();

  static std::string render(std::chrono::microseconds t);

  int argc_ = 0;
  char** argv_ = nullptr;
  char*  path_ = nullptr;
  bool colorize_ = false;
  const char* check_file_ = "<none>";
  size_t check_line_ = 0;
  test* current_test_ = nullptr;
  std::map<std::string, std::vector<std::unique_ptr<test>>> suites_;
  int max_runtime_ = 30; // 30s per default
};

namespace detail {

template <class T>
struct adder {
  adder(const char* suite_name, const char* test_name) {
    engine::add(suite_name, std::unique_ptr<T>{new T(test_name)});
  }
};

template <class T>
struct showable_base {
  explicit showable_base(const T& x) : value(x) {
    // nop
  }

  const T& value;
};

// showable_base<T> picks up to_string() via ADL
template <class T>
std::ostream& operator<<(std::ostream& out, const showable_base<T>& x) {
  auto str = caf::deep_to_string(x.value);
  if (str == "<unprintable>")
    out << term::blue << "<unprintable>" << term::reset;
  else
    out << str;
  return out;
}

template <class T>
class showable : public showable_base<T> {
public:
  explicit showable(const T& x) : showable_base<T>(x) {
    // nop
  }
};

// showable<T> picks up custom operator<< overloads for std::ostream
template <class T>
auto operator<<(std::ostream& out, const showable<T>& p)
-> decltype(out << std::declval<const T&>()) {
  return out << p.value;
}

template <class T>
showable<T> show(const T &x) {
  return showable<T>{x};
}

bool check(test* parent, const char *file, size_t line,
           const char *expr, bool should_fail, bool result);

template <class T, class U>
bool check(test* parent, const char *file, size_t line,
           const char *expr, bool should_fail, bool result,
           const T& x, const U& y) {
  auto out = logger::instance().massive();
  if (result) {
    out << term::green << "** "
        << term::blue << file << term::yellow << ":"
        << term::blue << line << fill(line) << term::reset
        << expr << '\n';
    parent->pass();
  } else {
    out << term::red << "!! "
        << term::blue << file << term::yellow << ":"
        << term::blue << line << fill(line) << term::reset
        << expr << term::magenta << " ("
        << term::red << show(x) << term::magenta
        << " !! " << term::red << show(y) << term::magenta
        << ')' << term::reset_endl;
    parent->fail(should_fail);
  }
  return result;
}

} // namespace detail
} // namespace test
} // namespace caf

// on the global namespace so that it can hidden via namespace-scoping
using caf_test_case_auto_fixture = caf::test::dummy_fixture;

#define CAF_TEST_PRINT(level, msg, colorcode)                                  \
  (::caf::test::logger::instance().level()                                     \
   << ::caf::term:: colorcode << "  -> " << ::caf::term::reset << msg          \
   << " [line " << __LINE__ << "]\n")

#define CAF_TEST_PRINT_ERROR(msg)   CAF_TEST_PRINT(info, msg, red)
#define CAF_TEST_PRINT_INFO(msg)    CAF_TEST_PRINT(info, msg, yellow)
#define CAF_TEST_PRINT_VERBOSE(msg) CAF_TEST_PRINT(verbose, msg, yellow)

#define CAF_PASTE_CONCAT(lhs, rhs) lhs ## rhs

#define CAF_PASTE(lhs, rhs) CAF_PASTE_CONCAT(lhs, rhs)

#define CAF_UNIQUE(name) CAF_PASTE(name, __LINE__)

#ifndef CAF_SUITE
#define CAF_SUITE unnamed
#endif

#define CAF_STR(s) #s

#define CAF_XSTR(s) CAF_STR(s)

#define CAF_FUNC_EXPR(func, x_expr, y_expr) #func "(" #x_expr ", " #y_expr ")"

#define CAF_ERROR(msg)                                                         \
  do {                                                                         \
    CAF_TEST_PRINT_ERROR(msg);                                                 \
    ::caf::test::engine::current_test()->fail(false);                          \
    ::caf::test::engine::last_check_file(__FILE__);                            \
    ::caf::test::engine::last_check_line(__LINE__);                            \
  } while (false)

#define CAF_CHECK(...)                                                         \
  do {                                                                         \
    static_cast<void>(::caf::test::detail::check(                              \
      ::caf::test::engine::current_test(), __FILE__, __LINE__,                 \
      #__VA_ARGS__, false, static_cast<bool>(__VA_ARGS__)));                   \
    ::caf::test::engine::last_check_file(__FILE__);                            \
    ::caf::test::engine::last_check_line(__LINE__);                            \
  } while(false)

#define CAF_CHECK_FUNC(func, x_expr, y_expr)                                   \
  do {                                                                         \
    func comparator;                                                           \
    const auto& x_val___ = x_expr;                                             \
    const auto& y_val___ = y_expr;                                             \
    static_cast<void>(::caf::test::detail::check(                              \
      ::caf::test::engine::current_test(), __FILE__, __LINE__,                 \
      CAF_FUNC_EXPR(func, x_expr, y_expr), false,                              \
      comparator(x_val___, y_val___), x_val___, y_val___));                    \
    ::caf::test::engine::last_check_file(__FILE__);                            \
    ::caf::test::engine::last_check_line(__LINE__);                            \
  } while (false)

#define CAF_CHECK_FAIL(...)                                                    \
  do {                                                                         \
    static_cast<void>(::caf::test::detail::check(                              \
      ::caf::test::engine::current_test(), __FILE__, __LINE__,                 \
      #__VA_ARGS__, true, static_cast<bool>(__VA_ARGS__)));                    \
    ::caf::test::engine::last_check_file(__FILE__);                            \
    ::caf::test::engine::last_check_line(__LINE__);                            \
  } while(false)

#define CAF_FAIL(msg)                                                          \
  do {                                                                         \
    CAF_TEST_PRINT_ERROR(msg);                                                 \
    ::caf::test::engine::current_test()->fail(false);                          \
    ::caf::test::detail::requirement_failed("test failure");                   \
  } while (false)

#define CAF_REQUIRE(...)                                                       \
  do {                                                                         \
    auto CAF_UNIQUE(__result) = ::caf::test::detail::check(                    \
      ::caf::test::engine::current_test(), __FILE__, __LINE__, #__VA_ARGS__,   \
      false, static_cast<bool>(__VA_ARGS__));                                  \
    if (!CAF_UNIQUE(__result))                                                 \
      ::caf::test::detail::requirement_failed(#__VA_ARGS__);                   \
    ::caf::test::engine::last_check_file(__FILE__);                            \
    ::caf::test::engine::last_check_line(__LINE__);                            \
  } while (false)

#define CAF_REQUIRE_FUNC(func, x_expr, y_expr)                                 \
  do {                                                                         \
    func comparator;                                                           \
    const auto& x_val___ = x_expr;                                             \
    const auto& y_val___ = y_expr;                                             \
    auto CAF_UNIQUE(__result) = ::caf::test::detail::check(                    \
      ::caf::test::engine::current_test(), __FILE__, __LINE__,                 \
      CAF_FUNC_EXPR(func, x_expr, y_expr), false,                              \
      comparator(x_val___, y_val___), x_val___, y_val___);                     \
    if (!CAF_UNIQUE(__result))                                                 \
      ::caf::test::detail::requirement_failed(                                 \
        CAF_FUNC_EXPR(func, x_expr, y_expr));                                  \
    ::caf::test::engine::last_check_file(__FILE__);                            \
    ::caf::test::engine::last_check_line(__LINE__);                            \
  } while (false)

#define CAF_TEST(name)                                                         \
  namespace {                                                                  \
  struct CAF_UNIQUE(test) : caf_test_case_auto_fixture {                       \
    void run();                                                                \
  };                                                                           \
  ::caf::test::detail::adder< ::caf::test::test_impl<CAF_UNIQUE(test)>>        \
  CAF_UNIQUE(a) {CAF_XSTR(CAF_SUITE), CAF_XSTR(name)};                         \
  } /* namespace <anonymous> */                                                \
  void CAF_UNIQUE(test)::run()

#define CAF_TEST_FIXTURE_SCOPE(scope_name, fixture_name)                       \
  namespace scope_name { using caf_test_case_auto_fixture = fixture_name ;

#define CAF_TEST_FIXTURE_SCOPE_END()                                           \
  } // namespace <scope_name>

// -- Convenience macros -------------------------------------------------------

#define CAF_MESSAGE(msg) CAF_TEST_PRINT_VERBOSE(msg)

// -- CAF_CHECK* predicate family ----------------------------------------------

#define CAF_CHECK_EQUAL(x, y)                                                  \
  CAF_CHECK_FUNC(::caf::test::equal_to, x, y)

#define CAF_CHECK_NOT_EQUAL(x, y)                                              \
  CAF_CHECK_FUNC(::caf::test::not_equal_to, x, y)

#define CAF_CHECK_LESS(x, y)                                                   \
  CAF_CHECK_FUNC(::caf::test::less_than, x, y)

#define CAF_CHECK_NOT_LESS(x, y)                                               \
  CAF_CHECK_FUNC(::caf::test::negated<::caf::test::less_than>, x, y)

#define CAF_CHECK_LESS_OR_EQUAL(x, y)                                          \
  CAF_CHECK_FUNC(::caf::test::less_than_or_equal, x, y)

#define CAF_CHECK_NOT_LESS_OR_EQUAL(x, y)                                      \
  CAF_CHECK_FUNC(::caf::test::negated<::caf::test::less_than_or_equal>, x, y)

#define CAF_CHECK_GREATER(x, y)                                                \
  CAF_CHECK_FUNC(::caf::test::greater_than, x, y)

#define CAF_CHECK_NOT_GREATER(x, y)                                            \
  CAF_CHECK_FUNC(::caf::test::negated<::caf::test::greater_than>, x, y)

#define CAF_CHECK_GREATER_OR_EQUAL(x, y)                                       \
  CAF_CHECK_FUNC(::caf::test::greater_than_or_equal, x, y)

#define CAF_CHECK_NOT_GREATER_OR_EQUAL(x, y)                                   \
  CAF_CHECK_FUNC(::caf::test::negated<::caf::test::greater_than_or_equal>, x, y)

// -- CAF_CHECK* predicate family ----------------------------------------------

#define CAF_REQUIRE_EQUAL(x, y)                                                \
  CAF_REQUIRE_FUNC(::caf::test::equal_to, x, y)

#define CAF_REQUIRE_NOT_EQUAL(x, y)                                            \
  CAF_REQUIRE_FUNC(::caf::test::not_equal_to, x, y)

#define CAF_REQUIRE_LESS(x, y)                                                 \
  CAF_REQUIRE_FUNC(::caf::test::less_than, x, y)

#define CAF_REQUIRE_NOT_LESS(x, y)                                             \
  CAF_REQUIRE_FUNC(::caf::test::negated<::caf::test::less_than>, x, y)

#define CAF_REQUIRE_LESS_OR_EQUAL(x, y)                                        \
  CAF_REQUIRE_FUNC(::caf::test::less_than_or_equal, x, y)

#define CAF_REQUIRE_NOT_LESS_OR_EQUAL(x, y)                                    \
  CAF_REQUIRE_FUNC(::caf::test::negated<::caf::test::less_than_or_equal>, x, y)

#define CAF_REQUIRE_GREATER(x, y)                                              \
  CAF_REQUIRE_FUNC(::caf::test::greater_than, x, y)

#define CAF_REQUIRE_NOT_GREATER(x, y)                                          \
  CAF_REQUIRE_FUNC(::caf::test::negated<::caf::test::greater_than>, x, y)

#define CAF_REQUIRE_GREATER_OR_EQUAL(x, y)                                     \
  CAF_REQUIRE_FUNC(::caf::test::greater_than_or_equal, x, y)

#define CAF_REQUIRE_NOT_GREATER_OR_EQUAL(x, y)                                 \
  CAF_REQUIRE_FUNC(::caf::test::negated<::caf::test::greater_than_or_equal>,   \
                   x, y)

#endif // CAF_TEST_UNIT_TEST_HPP
